---
eyebrow: 'Docs · Getting started'
lede: 'Where data lives on the chip, how the Zend engine is embedded, and what happens between reset and the first byte of output. The worked example is the ESP32-P4; a closing section covers the ESP32-S3.'
see_also:
  - href: ../reference/footprint.md
    meta: 'Static size of the image and where each region lands'
  - href: ../reference/porting-notes.md
    meta: 'Engine-level porting decisions and the web-server SAPI in detail'
  - href: ../extensions/porting-status.md
    meta: 'Which bundled extensions are compiled in'
  - href: https://github.com/php-baremetal/php-esp32
    meta: 'Source repository'
    label: 'php-baremetal/php-esp32 on GitHub'
prev: { label: 'Getting started', href: './quick-start.md' }
next: { label: 'Extension porting status', href: '../extensions/porting-status.md' }
---

# Architecture: memory and execution

This page describes two things at once: where code and data physically live on the chip,
and what the firmware does from reset to the first byte of output. The important idea to
hold onto is that this is a real, unmodified Zend engine running the same compile-then-execute
pipeline it runs on a server. What changes is the environment around it: a few hundred KB of
fast internal SRAM, a large but slower PSRAM heap, a single CPU core, code executed straight
out of flash, and no operating system underneath other than FreeRTOS.

The worked example throughout is the ESP32-P4. The ESP32-S3 has the same shape with different
sizes and peripherals; the differences are collected in the last section.

## Memory map

The chip has four distinct places where code and data live, each with a role. The single most
important fact is that code is not copied into RAM to run. It executes straight from flash
through the MMU cache — execute in place, "XIP" — which is why a few hundred KB of internal
SRAM is enough to run a multi-megabyte image. Internal SRAM is small and fast; PSRAM is large
and holds everything that grows at runtime.

| Region | Size (P4) | Speed | What lives there |
| --- | --- | --- | --- |
| NOR flash `.flash.text` | ~2.1 MB | XIP via MMU cache | PHP + ESP-IDF code, executed in place |
| NOR flash `.flash.rodata` | ~750 KB | XIP via MMU cache | Read-only constants |
| Internal SRAM `.iram0.text` | ~72 KB | fast on-chip | Code that must not fault on a cache miss (ISRs) |
| Internal SRAM `.dram0.data` | ~15 KB | fast on-chip | Initialized globals |
| Internal SRAM `.dram0.bss` | ~97 KB | fast on-chip | Uninitialized engine globals (largest: the `crypt` tables) |
| Internal SRAM (PHP task stack) | 64 KB | fast on-chip | The `php` FreeRTOS task's stack |
| Internal SRAM (internal heap) | remainder of 768 KB | fast on-chip | DMA buffers, FreeRTOS objects |
| PSRAM | up to 32 MB | slower, off-chip | The PHP runtime heap: zvals, HashTables, compiled opcodes, the object graph |
| microSD | card-dependent | slow, block I/O | `index.php` and writable data (read at boot) |

The static footprint is modest. About 97 KB of uninitialized data plus a handful of KB of
initialized data sits well inside the 768 KB of internal SRAM. Everything that grows while a
script runs lives in PSRAM instead. See [footprint.md](../reference/footprint.md) for the
exact numbers of the current build.

<!-- @callout variant="info" title="Execute in place (XIP)" -->
Code runs directly from flash through the MMU cache rather than being loaded into RAM. A cache
miss stalls until the line is fetched from flash, but the working set of hot code stays cached,
so the amortized cost is low. The upshot for the memory budget: image size is bounded by the
flash partition, not by RAM.
<!-- @endcallout -->

## Why PHP allocates into PSRAM

PSRAM is where PHP allocates, and this is deliberate at two levels.

First, Zend's own memory manager is switched off. The firmware sets `USE_ZEND_ALLOC=0` in the
environment before the engine starts, which tells Zend to route every allocation through plain
`malloc`/`free` instead of its internal arena allocator.

<!-- @code-block language="c" label="php_task(): disable the Zend allocator" -->
```c
/* PHP allocations go to PSRAM (see CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0), which
 * keeps internal RAM free for DMA and FreeRTOS. */
setenv("USE_ZEND_ALLOC", "0", 1);
```
<!-- @endcode-block -->

Second, ESP-IDF is configured to send `malloc` to PSRAM. The board's chip-level defaults enable
external PSRAM as a `malloc` region and, critically, set the internal-allocation threshold to
zero so that *all* allocations — not just large ones — land in PSRAM.

<!-- @code-block language="ini" label="boards/esp32-p4/sdkconfig.family" -->
```ini
# 32 MB in-package PSRAM. This is where the runtime heap lives: with
# USE_ZEND_ALLOC=0 the engine uses malloc, and SPIRAM_USE_MALLOC routes large
# allocations to PSRAM.
CONFIG_SPIRAM=y
CONFIG_SPIRAM_USE_MALLOC=y
# Send ALL malloc() allocations to PSRAM (threshold 0), not just large ones.
# PHP makes thousands of small allocations; with the default 16 KB threshold they
# land in internal RAM and exhaust it, starving DMA (the SD card) and FreeRTOS.
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0
```
<!-- @endcode-block -->

<!-- @callout variant="warning" title="Keeping PHP out of internal SRAM is a requirement, not a tuning knob" -->
PHP makes thousands of small allocations. With the default 16 KB "always internal" threshold,
those small allocations would land in internal SRAM and exhaust it — starving DMA (which the SD
card needs) and FreeRTOS objects, neither of which can live in PSRAM. Setting the threshold to
zero forces the function and class tables, the zvals, the compiled opcodes and the object graph
into PSRAM, where there is room, and leaves internal RAM free for the things that must have it.
Internal-only needs still get internal memory through explicit heap-capability allocations.
<!-- @endcallout -->

## Why the PHP task stack is large

The engine runs on its own FreeRTOS task with a 64 KB stack. That is large for an embedded
task, and for two concrete reasons: the PHP compiler recurses heavily while descending a syntax
tree, and `zend_bailout` unwinds fatal errors with `setjmp`/`longjmp`, which needs the frames it
jumps over to still be on the stack.

<!-- @code-block language="c" label="main.c: the task and its stack" -->
```c
/* 64 KB: with a smaller stack the board resets on trivial scripts.
 * ESP-IDF's xTaskCreate takes the stack size in bytes. */
#define PHP_TASK_STACK_BYTES (64 * 1024)

void app_main(void)
{
    ESP_LOGI(TAG, "starting PHP runtime");
    xTaskCreate(php_task, "php", PHP_TASK_STACK_BYTES, NULL, 5, NULL);
}
```
<!-- @endcode-block -->

`app_main()` does nothing but spawn this task. Everything else — mounting storage, bringing up
the network, initializing the engine, running the script — happens inside `php_task`.

## Execution flow: from reset to output

The first steps are the ordinary ESP-IDF boot sequence. Everything after `app_main()` is the
firmware's own pipeline.

<!-- @steps -->
1. **Reset** hands control to the ROM bootloader.
2. The **second-stage bootloader** in flash loads the app partition and jumps to it.
3. **`app_main()`** runs from flash and creates the FreeRTOS `php` task (64 KB stack).
4. Inside the task: **mount storage** (microSD and/or the embedded FAT image) and, on a
   networked board, **bring the link up** and log the address.
5. Resolve the **entry script** (embedded source takes priority over the SD card) and set up
   optional features: OPcache ini defaults, `OPENSSL_CONF`, the TLS CA bundle, the baked `.env`.
6. **`php_embed_init()`** brings the Zend engine up.
7. **Register per-project C extensions**, then run the entry script under the selected
   execution model.
<!-- @endsteps -->

Concretely, take `<?php echo 1 + 1;`. The source goes into `zend_compile_string()`, which
tokenizes it, builds a syntax tree, and lowers it to opcodes — the instructions of PHP's virtual
machine. The opcodes live in the PSRAM heap. `zend_execute()` then runs the VM: a loop that
takes one opcode at a time and calls the C function implementing it (`ZEND_ADD`, `ZEND_ECHO`,
and the rest). `ZEND_ECHO` reaches the SAPI's output funnel, `ub_write`, and from there the
bytes go to the serial console or, under the web-server model, into the HTTP response.

It is exactly the path the code takes on a server. Here it runs on a single core at a few
hundred MHz instead of on a PC.

## The portable "call" VM

Zend can generate several dispatch variants for its virtual machine. The common one on desktop
builds is the "goto" variant, which uses GCC's computed-goto (labels-as-values) to thread from
one opcode handler to the next. That variant does not compile cleanly on these targets, so this
build uses the portable **"call"** variant instead: the dispatch loop is an ordinary `switch`/
function-call over opcodes. The engine's behaviour is identical; only the dispatch mechanism
differs. The practical consequence is portability — the same engine source builds for RISC-V
(P4) and Xtensa (S3) without touching any VM code.

## The embed SAPI

PHP always runs behind a SAPI (Server API): the CLI SAPI, the FPM SAPI, Apache's module, and so
on. This firmware embeds PHP through the **embed SAPI**, the minimal library interface meant for
hosting the engine inside another C program. `php_embed_init()` starts the engine and opens one
request; `php_embed_shutdown()` tears it down.

The firmware customizes the embed module in a few places. The default output sink is replaced so
that engine output goes straight to the console, always reporting the full length written.

<!-- @code-block language="c" label="main.c: the run-once output sink" -->
```c
/*
 * Output sink for the engine: echo, print, printf, var_dump and php_printf() all
 * funnel through here. Write straight to the console and always report the full
 * length. The embed SAPI's default ub_write treats a short write as a dropped
 * connection, which fires php_handle_aborted_connection() -> zend_bailout() ->
 * exit(); on this target exit() then aborts inside newlib.
 */
static size_t esp_ub_write(const char *str, size_t len)
{
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}
```
<!-- @endcode-block -->

The engine has no `dlopen`: `HAVE_LIBDL` is undefined and there are no POSIX signals
(`ZEND_SIGNALS` is off) under FreeRTOS/newlib. A few other embed hooks are set before
`php_embed_init()` so the values take effect from the first line of the script:

- **`ini_defaults`** seeds OPcache directives (which are `PHP_INI_SYSTEM` and cannot be set at
  runtime). See [opcache.md](../extensions/opcache.md).
- **`name`** is changed to `cli-server` in the web-server model so frameworks treat each run as
  an HTTP request rather than a CLI invocation.
- **environment** (`OPENSSL_CONF`, the TLS CA path, and the baked project `.env`) is applied
  with `setenv()` before init so it appears in `$_ENV`/`getenv()`.

<!-- @callout variant="note" title="Why banners use printf, not php_printf" -->
Firmware banners are written straight to `stdout`, never through `php_printf`. `php_printf` runs
PHP's output layer, which marks `SG(headers_sent)` — and once headers are considered sent,
`session_start()`, `setcookie()`, `header()` and the session ini settings all refuse to run. The
console output is identical either way because `esp_ub_write` also just writes `stdout`.
<!-- @endcallout -->

## The static-extension model

A normal PHP build discovers extensions at configure time and can load some at runtime via
`dlopen`. This build does neither. There is no dynamic loading; every extension is compiled in
and linked statically. The list is hand-written (because `configure` is not run) as a
`php_builtin_extensions[]` table that `php_register_internal_extensions()` walks at startup.

<!-- @code-block language="c" label="internal_functions.c: the static extension table" -->
```c
static zend_module_entry * const php_builtin_extensions[] = {
#ifdef PHP_EXT_DATE_ENABLED
    &date_module_entry,
#endif
    phpext_pcre_ptr,
    phpext_hash_ptr,
    phpext_json_ptr,
    phpext_random_ptr,
    phpext_reflection_ptr,
    phpext_standard_ptr,
    phpext_spl_ptr,
    /* ... optional ctype/mbstring/filter/tokenizer/session/openssl/pdo ... */
    &gpio_module_entry,
    &store_module_entry,
    &mem_module_entry,
};

PHPAPI int php_register_internal_extensions(void)
{
    return php_register_extensions(php_builtin_extensions, EXTCOUNT);
}
```
<!-- @endcode-block -->

The rules of this model:

- The table **must match** the extensions actually compiled. A name listed but not built fails
  the link with an unresolved `phpext_<name>_ptr`.
- Optional extensions are gated by `PHP_EXT_*_ENABLED` macros set from the build configuration,
  so the table shrinks and grows with what the project selected.
- `date` is left out of the default set because it pulls in timelib and the timezone database;
  a lightweight stub stands in unless `PHP_EXT_DATE_ENABLED` is set.
- Ordering matters where one module depends on another — `pdo` is registered before
  `pdo_sqlite`.
- The three trailing entries — `gpio`, `store`, `mem` — are this project's own built-in
  hardware/persistence extensions, always present. See
  [persistent-store.md](../storage/persistent-store.md) and
  [in-ram-store.md](../storage/in-ram-store.md).

Which of the optional bundled extensions are available, and their current status, is tracked in
[porting-status.md](../extensions/porting-status.md).

### Per-project C extensions

Beyond the built-in table, a project can drop its own C extensions into `./firmware/exts/<name>/`.
The `php_project_exts` component compiles them and generates a table of `zend_module_entry`
pointers, linked with whole-archive. These are registered *after* `php_embed_init()`, so their
functions, classes and constants are available to the script — MINIT runs, though there is no
per-request RINIT for a module added this late, which a hardware-driver extension does not need.
The symbols are weak, so a firmware built with no project extensions still links and the count
resolves to zero. This is covered in [custom-extensions.md](../extensions/custom-extensions.md).

<!-- @code-block language="c" label="main.c: registering project extensions" -->
```c
extern zend_module_entry * const php_esp32_project_extensions[] __attribute__((weak));
extern const int php_esp32_project_extension_count __attribute__((weak));

static void register_project_extensions(void)
{
    if (&php_esp32_project_extension_count == NULL || php_esp32_project_extension_count == 0) {
        return;
    }
    for (int i = 0; i < php_esp32_project_extension_count; i++) {
        zend_module_entry *m = php_esp32_project_extensions[i];
        if (zend_startup_module(m) == SUCCESS) {
            ESP_LOGI(TAG, "project ext '%s' registered", m->name);
        }
    }
}
```
<!-- @endcode-block -->

## The three execution models

The firmware supports three shapes of program. The first two share the same build (the default,
"run-script + setup/loop"); the third is a separate build type selected with
`-DPHP_PROJECT_WEB_SERVER=ON`.

<!-- @tabs labels="Run once, setup/loop, web-server" -->
<!-- @tab index="0" -->
**Run once.** A plain, linear script runs top to bottom and returns. Before it runs, the
firmware clears `SG(headers_sent)` (so `session_start()` / `header()` / `setcookie()` work) and
materializes a minimal `$_SERVER` resembling a plain `GET /`, so a framework front controller can
capture a sane request instead of guessing from empty globals. The whole run is wrapped in
`zend_try`/`zend_catch` so a fatal error or `die()` is caught rather than reaching `exit()`.

<!-- @code-block language="php" label="A plain run-once script" -->
```php
<?php
echo "Hello from " . PHP_OS . "\n";
echo 1 + 1, "\n";
```
<!-- @endcode-block -->
<!-- @endtab -->
<!-- @tab index="1" -->
**setup() / loop() (Arduino-style).** If the entry script defines a `loop()` function, the file
is run once (which defines the functions), then C calls `setup()` once and `loop($tick)`
repeatedly. Keeping the loop in C is deliberate: that is where the cycle collector runs
periodically, where each call into PHP is wrapped in `zend_try`/`zend_catch` (so a fatal error is
logged and the loop continues instead of resetting the board), and where an uncaught exception is
cleared and logged. `delay()` inside `loop()` maps to `vTaskDelay`, which yields the core so the
watchdog stays satisfied — a `loop()` that never delays will trip it.

<!-- @code-block language="c" label="main.c: driving loop() from C" -->
```c
for (uint32_t tick = 0; ; tick++) {
    zval arg; ZVAL_LONG(&arg, tick);
    zval ret; ZVAL_UNDEF(&ret);
    zend_call_known_function(fn_loop, NULL, NULL, &ret, 1, &arg, NULL);
    zval_ptr_dtor(&ret);

    if (EG(exception)) {
        zend_clear_exception();   /* log-and-continue */
    }
    /* Refcounting frees most garbage immediately; cycles need a periodic sweep. */
    if ((tick & 0xFF) == 0) {
        gc_collect_cycles();
        ESP_LOGI(TAG, "tick %u -- heap free: %u bytes",
                 (unsigned) tick, (unsigned) esp_get_free_heap_size());
    }
}
```
<!-- @endcode-block -->
<!-- @endtab -->
<!-- @tab index="2" -->
**web-server.** Selected at build time with `-DPHP_PROJECT_WEB_SERVER=ON`. A C HTTP server
(`esp_http_server`) sits in front, and PHP runs fresh for each request — shared-nothing, the way
a script runs behind nginx + PHP-FPM. Each incoming request is turned into a full CGI-style
`$_SERVER` / `$_GET` / `$_POST` / `$_COOKIE`, the front controller runs, and the output plus the
headers/status/cookies it set become the HTTP response. This is enough to drive a real framework
(Laravel, Symfony) as a browsable app — routing, sessions, forms. The engine stays up across
requests; only the request state is torn down and rebuilt.
<!-- @endtab -->
<!-- @endtabs -->

## Inside the web-server model

The web-server model has enough moving parts to be worth a closer look. Its defining constraint
is that **PHP must run on the task that has the 64 KB stack**, not on the HTTP server's task.

`php_embed_init()` opens one request; the firmware closes that opened request and then cycles a
fresh `php_request_startup()` → run the entry script → `php_request_shutdown()` for every HTTP
request. Because each request is shared-nothing, re-running the top-level script every time does
not trip redeclaration errors.

### Two tasks, a shared slot, two semaphores

The HTTP server runs its handler on the small-stacked `httpd` task; PHP runs on `php_task`. They
hand a single request back and forth through two binary semaphores:

<!-- @steps -->
1. The `httpd` handler parses the request off the socket into a single static `ws_request_t`
   (method, URI, query, headers, cookies, and the POST body — read here, on the task that owns
   the socket).
2. It serves the request directly if the path maps to an existing static file under the document
   root (`public/`) — no PHP cycle, exactly like `try_files $uri /index.php`. `.php` files,
   directories, `/`, and paths containing `..` are never served this way.
3. Otherwise it gives `s_ws_req_ready`, waking `php_task`, and blocks on `s_ws_resp_ready`.
4. `php_task` runs one full request cycle: it fills `SG(request_info)` before startup (so `$_GET`
   / `$_POST` see the query and body), calls `php_request_startup()`, runs the script under
   `zend_try`/`zend_catch`, flushes any headers the script set, and calls `php_request_shutdown()`.
   Output is appended to a growing response buffer by the SAPI `ub_write` hook.
5. `php_task` gives `s_ws_resp_ready`; the `httpd` task wakes and sends the captured status,
   headers and body, then frees the body buffer.
<!-- @endsteps -->

The httpd server handles one request at a time, so the single shared `ws_request_t`, the single
response buffer and the header-capture buffers are all safe. Neither task touches the socket
while the other is using it.

### Per-request SAPI hooks

For the duration of the web server, the live `sapi_module` copy is pointed at a set of
request-scoped hooks (the copy `sapi_startup()` made at init time):

| Hook | Role |
| --- | --- |
| `ub_write` | Append script output to the per-request response buffer (grows via `realloc`) |
| `send_headers` | Translate the status/headers the script set into the httpd response, copying each so it survives request shutdown |
| `read_post` | Feed PHP the POST body for `$_POST` / `php://input` |
| `read_cookies` | Hand PHP the raw `Cookie` header for `$_COOKIE` (sessions) |
| `register_server_variables` | Build the full CGI-style `$_SERVER` |

<!-- @code-block language="c" label="run_web_server(): wiring the request hooks" -->
```c
sapi_module.ub_write                  = ws_ub_write;
sapi_module.send_headers              = ws_send_headers;
sapi_module.read_post                 = ws_read_post;
sapi_module.read_cookies              = ws_read_cookies;
sapi_module.register_server_variables = ws_register_server_vars;
php_request_shutdown(NULL);   /* end the request embed_init opened; module stays up */
```
<!-- @endcode-block -->

### One-time init script

The web-server build accepts an optional init script (`[web-server] init`) that runs once, after
`php_embed_init()` and before the HTTP server starts, in the request that init already opened —
so its output goes to the console. Its effects that live below PHP (C-extension state, `mem_` or
`store_` values) are then shared by every later request. A failure is logged but not fatal; the
server still comes up.

Full engine-level detail on the SAPI is in [porting-notes.md](../reference/porting-notes.md).

## The board.h HAL

`main.c` is board-agnostic. Everything specific to a board's wiring — the microSD pins and power,
the Ethernet controller, the mount and network-bring-up code — lives behind a small interface in
each board's `board.h` and its `board.c`. `main.c` includes `board.h` and talks only to this
contract; it does not know whether storage is 4-bit SDIO or SPI, or whether the network is an
internal MAC or an external SPI chip.

The interface has two parts: capability macros that the firmware compiles against, and functions
that each board implements.

| Symbol | Kind | Meaning |
| --- | --- | --- |
| `BOARD_NAME` | macro (string) | The specific board's identity, used by tooling (e.g. `"ESP32-P4-ETH"`) |
| `BOARD_SOC` | macro (string) | The SoC/family, printed in the console banner (e.g. `"ESP32-P4"`) |
| `BOARD_HAS_NETWORK` | macro (defined/undef) | If defined, the firmware brings the link up at boot and logs the address |
| `BOARD_HAS_MICROSD` | macro (defined/undef) | If defined, the board has a card slot and provides `board_mount_storage()` |
| `board_mount_storage(mount_point)` | function → `bool` | Mount the board's storage at `mount_point`; true on success |
| `board_unmount_storage(mount_point)` | function → `void` | Unmount what `board_mount_storage()` mounted (no-op if nothing is mounted) |
| `board_network_up(ip_out, ip_len)` | function → `bool` | Bring the network up, wait a bounded time for a DHCP lease, write the dotted-decimal IPv4 into `ip_out` (≥16 bytes); true on success |

The macros gate whole code paths at compile time. A `-zero` board with no card slot leaves
`BOARD_HAS_MICROSD` undefined, so the SD path is never built or probed; a build that asked for
microSD on such a board fails at compile time with a clear `#error` rather than at link time with
a missing `board_mount_storage()`. Boards without networking leave `BOARD_HAS_NETWORK` undefined,
and the netif, DNS and network-bring-up code is compiled out entirely.

<!-- @method name="board_mount_storage" returns="bool" visibility="public" / -->

<!-- @params -->
<!-- @param name="mount_point" type="const char *" -->
The VFS path to mount the card at (the firmware uses `/sdcard`). On the P4-ETH this is a 4-bit
SDMMC microSD powered by the on-chip LDO, with its VDD switch (GPIO45) enabled first; on the
S3-ETH it is a microSD in SPI mode.
<!-- @endparam -->
<!-- @endparams -->

<!-- @method name="board_network_up" returns="bool" visibility="public" / -->

<!-- @params -->
<!-- @param name="ip_out" type="char *" -->
Buffer (≥16 bytes) that receives the dotted-decimal IPv4 address on success.
<!-- @endparam -->
<!-- @param name="ip_len" type="size_t" -->
Size of `ip_out`. Returns false if the link stays down or no lease arrives in the bounded wait.
<!-- @endparam -->
<!-- @endparams -->

<!-- @callout variant="tip" title="Storage: two independent sources" -->
The firmware can mount two things at once. The microSD at `/sdcard` holds writable data (SQLite,
logs, files the script writes). A read-only FAT image in the internal `storage` flash partition,
mounted at `/app`, holds the embedded PHP source when the firmware is built for "embedded"
storage. `index.php` runs from the embedded source if present, otherwise from the card — and an
embedded project can still use the card for its data.
<!-- @endcallout -->

## Build-time patches

The stock PHP sources are patched at build time. The patches are small and each addresses one
portability point; they are applied per PHP version under `components/php/versions/<ver>/patches/php/`.
The current set:

| Patch | Purpose |
| --- | --- |
| `0001-closure-runtime-cache-arena` | Closure runtime-cache arena handling |
| `0002-ext-date-optional-minimal-tz` | Make `ext/date` optional with a minimal timezone footprint |
| `0003-mbstring-optional-no-cjk` | Make `mbstring` optional and drop the CJK tables |
| `0004-csprng-esp-getrandom` | Back the CSPRNG with the ESP hardware RNG |
| `0005-session-files-no-cloexec-warn` | Silence a `O_CLOEXEC` warning from the files session handler |
| `0006-opcache-static-embed` | Let OPcache link statically into the embed build |
| `0007-opcache-malloc-shm-backend` | A `malloc`-based SHM backend so the in-RAM OPcache lives in PSRAM |
| `0008-zend-portability-sigsetjmp-guard` | Guard `sigsetjmp` where POSIX signals are unavailable |
| `0009-uri-drop-lexbor-module-dep` | (8.5) Drop the lexbor dependency from `ext/uri` |

Everything else — the engine, the VM, the bundled extensions — is unmodified PHP. This is why a
version bump is mostly a matter of regenerating these patches against the new tree rather than
re-porting the engine.

## How the ESP32-S3 differs

The shape is the same; the sizes and a couple of peripherals change.

- **CPU / toolchain.** The S3 is dual-core Xtensa LX7 rather than the P4's RISC-V, so PHP is
  built with the `xtensa-esp32s3-elf` toolchain. The portable "call" VM means no engine code
  changes.
- **Memory.** 8 MB of PSRAM instead of up to 32 MB, and 16 MB of flash instead of 32 MB. The
  same firmware and heap fit with less headroom: plain applications and a live web server run
  comfortably; a full framework's container-compile step does not.
- **Storage.** microSD over SPI (`SD_MOSI=6, SD_MISO=5, SD_CLK=7, SD_CS=4`) rather than the P4's
  4-bit SDIO — the S3 has no internal SD host.
- **Network.** On the S3-ETH the wired network is a W5500 SPI Ethernet controller (MAC+PHY in one
  chip) rather than an internal MAC with an RMII PHY; the SD card and the W5500 sit on separate
  SPI hosts so they do not share pins.

All of that is contained in the board's `board.c` and `board.h`. `main.c` and the engine do not
know the difference — the same `board_mount_storage()` / `board_network_up()` contract holds, and
the same `USE_ZEND_ALLOC=0` → `malloc` → PSRAM routing applies (the S3 family config sets the same
`CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0`).
