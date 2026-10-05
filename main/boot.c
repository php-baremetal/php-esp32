/*
 * boot.c -- firmware entry and the PHP runtime bootstrap.
 *
 * app_main() brings up the reactor task (php_task, in php_task.c), pinned to PHP_TASK_CORE. The task
 * then calls boot_php_runtime() here, which does the whole setup: mount the microSD and the embedded
 * source, raise the network, resolve the entry script, install the embed SAPI hooks (output, SAPI
 * name, OPcache), apply the baked .env, fill the identity globals, start the engine and register the
 * project extensions. It publishes the entry + source mount in g_entry_script / g_src_dir (app.h) for
 * the model runner. boot_php_shutdown() reverses it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>   /* mkdir (opcache file-cache dir) */

#include "esp_log.h"
#include "esp_system.h"      /* esp_get_free_heap_size */
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"    /* esp_app_get_description()->version -- the php-esp32 firmware version */
#include "esp_idf_version.h" /* esp_get_idf_version() */
#ifdef PHP_POWER_SAVE_ENABLED
#include "esp_pm.h"          /* esp_pm_configure -- automatic light sleep (power_save) */
#endif

#include "board.h"   /* board_mount_storage()/board_unmount_storage(), BOARD_NAME, BOARD_SOC, BOARD_HAS_NETWORK */

/* microSD support was requested for a build, but the selected board has no card slot
 * (it does not define BOARD_HAS_MICROSD). Fail here with a clear message instead of later
 * with an undefined board_mount_storage(). Reachable via `[storage] microsd = true` on an
 * embedded-only board like esp32-*-zero. */
#if defined(PHP_STORAGE_MICROSD) && !defined(BOARD_HAS_MICROSD)
#error "microSD requested but this board has no card slot: build an embedded project without [storage] microsd = true, or pick a board with a microSD slot."
#endif

#ifdef BOARD_HAS_NETWORK
#include "esp_netif.h"   /* after board.h: BOARD_HAS_NETWORK is defined there */
#endif

#include "php_embed.h"
#include "zend_API.h"

#include "boot.h"
#include "app.h"   /* g_entry_script / g_src_dir, s_board_ip, php_esp32_* -- populated here */
#include "event_bus_php.h"   /* bm_events_set_power_save */

static const char *TAG = "php-esp32";

/* Two independent sources, mounted together when both are present:
 *   - the microSD at /sdcard: writable data (SQLite, logs, files the script writes).
 *   - the embedded PHP source at /app: a read-only FAT image in internal flash, built
 *     only when the firmware is made for "embedded" storage. index.php runs from here if
 *     present, otherwise from the card -- and an embedded project can still use the card
 *     for its data. */
#define SD_MOUNT_POINT  "/sdcard"
#define APP_MOUNT_POINT "/app"
/* The entry script within the source, from [php] entry (default index.php). A framework whose
 * front controller is nested sets it -- e.g. Laravel: PHP_ENTRY="public/index.php". flash-tool
 * passes it as -DPHP_ENTRY; the paths are compile-time string concatenations. */
#ifndef PHP_ENTRY
#define PHP_ENTRY "index.php"
#endif
#define SD_SCRIPT       SD_MOUNT_POINT "/" PHP_ENTRY
#define APP_SCRIPT      APP_MOUNT_POINT "/" PHP_ENTRY

/* microSD mount state, remembered so boot_php_shutdown() can unmount what boot_php_runtime() mounted. */
#ifdef PHP_STORAGE_MICROSD
static bool s_have_sd;
#endif

#ifdef BOARD_HAS_NETWORK
/* Apply static DNS servers (a ","-separated list) to the default netif, overriding whatever DHCP
 * handed out. Up to two are used (lwIP keeps a main + a backup); extras and blanks are ignored.
 * An empty list is a no-op, leaving the DHCP-provided servers in place. */
static void net_apply_static_dns(const char *list)
{
    if (!list || !*list) {
        return;
    }
    esp_netif_t *netif = esp_netif_get_default_netif();
    if (!netif) {
        ESP_LOGW(TAG, "static DNS: no default netif");
        return;
    }
    char buf[128];
    strncpy(buf, list, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    int idx = 0;
    for (char *save = NULL, *tok = strtok_r(buf, ",", &save);
         tok && idx < 2;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;              /* trim leading spaces */
        if (!*tok) continue;
        esp_netif_dns_info_t dns = {0};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        if (esp_netif_str_to_ip4(tok, &dns.ip.u_addr.ip4) != ESP_OK) {
            ESP_LOGW(TAG, "static DNS: bad address '%s'", tok);
            continue;
        }
        esp_netif_dns_type_t t = (idx == 0) ? ESP_NETIF_DNS_MAIN : ESP_NETIF_DNS_BACKUP;
        if (esp_netif_set_dns_info(netif, t, &dns) == ESP_OK) {
            ESP_LOGI(TAG, "static DNS[%d] = %s", idx, tok);
            idx++;
        }
    }
}
#endif

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

/*
 * Per-project C extensions. A project can drop custom extensions in ./firmware/exts/<name>/;
 * the php_project_exts component compiles them and generates this table (linked with
 * WHOLE_ARCHIVE). The symbols are weak so a firmware built without any still links -- the count
 * then resolves to 0. Each entry is registered after php_embed_init(), so its functions, classes
 * and constants are available to the script (MINIT runs; there is no per-request RINIT for a
 * module added this late, which a hardware-driver extension doesn't need).
 */
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
        } else {
            ESP_LOGW(TAG, "project ext '%s' failed to register", m->name);
        }
    }
}

/*
 * Build-time environment. phpflash bakes the project's .env into this table (a flat array of
 * alternating key, value; see main/CMakeLists.txt and docs/environment.md). Applying it with
 * setenv() *before* php_embed_init() means PHP exposes it both as $_ENV[...] (variables_order carries
 * 'E') and via getenv(). An empty table (no .env) is a no-op.
 */
extern const char *const php_esp32_env[];
extern const int php_esp32_env_count;

static void apply_project_env(void)
{
    for (int i = 0; i < php_esp32_env_count; i++) {
        setenv(php_esp32_env[2 * i], php_esp32_env[2 * i + 1], 1);
    }
    if (php_esp32_env_count > 0) {
        ESP_LOGI(TAG, "applied %d env var(s) from .env", php_esp32_env_count);
    }
}

/* Mount the optional embedded PHP source: a read-only FAT image in the internal
 * 'storage' partition. Absent on microSD-only firmware (the partition may not exist, or
 * exist but hold no image) -- in which case this returns false and we run from the SD. */
static bool s_app_mounted;

static bool mount_embedded(void)
{
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");
    if (!p) {
        return false;
    }
    esp_vfs_fat_mount_config_t cfg = { .max_files = 5 };
    if (esp_vfs_fat_spiflash_mount_ro(APP_MOUNT_POINT, "storage", &cfg) != ESP_OK) {
        return false;
    }
    s_app_mounted = true;
    return true;
}

#ifdef PHP_EXT_OPCACHE_ENABLED
/* OPcache is compiled in (see docs/opcache.md). Its directives are PHP_INI_SYSTEM, so they can't be
 * set at runtime; we seed them here through the embed SAPI's ini_defaults hook, before startup. */
#ifndef PHP_OPCACHE_SHM_MODE
static char s_opcache_dir[96];   /* the writable file-cache dir, set once the card is mounted */
#endif

static void opc_add_ini_default(HashTable *ht, const char *name, const char *val)
{
    zval z;
    ZVAL_NEW_STR(&z, zend_string_init(val, strlen(val), 1));   /* persistent: freed by config dtor */
    zend_hash_str_update(ht, name, strlen(name), &z);
}

static void opcache_ini_defaults(HashTable *ht)
{
    opc_add_ini_default(ht, "opcache.enable",                  "1");
    opc_add_ini_default(ht, "opcache.enable_cli",             "1");   /* the embed SAPI is CLI-like */
    opc_add_ini_default(ht, "opcache.validate_timestamps",    "0");   /* code is static on the card */
    opc_add_ini_default(ht, "opcache.use_cwd",                "0");   /* all paths are absolute */
    /* No RTC/NTP: the clock sits at 1970 while the card's files are dated in the "future", so
     * OPcache's "file too new to cache" guard would skip every file. Disable it (validate_timestamps
     * is off anyway, so mtime doesn't matter). */
    opc_add_ini_default(ht, "opcache.file_update_protection", "0");
#ifdef PHP_OPCACHE_SHM_MODE
    /* In-RAM cache (opcache `in_memory` setting): the compiled bytecode stays in PSRAM (SHM backend,
     * shared_alloc_malloc.c) between requests, so after warm-up there's neither a recompile nor an
     * SD read. The catch: the whole bytecode plus the per-request heap must fit in the 32 MB PSRAM.
     * Fine for a small app; a large framework (Laravel) doesn't fit -- use the file cache for those.
     * memory_consumption is reserved up front, straight out of the per-request heap budget. */
    opc_add_ini_default(ht, "opcache.memory_consumption",      "16");   /* MB of PSRAM for the cache */
    opc_add_ini_default(ht, "opcache.interned_strings_buffer", "2");    /* MB, carved from the above */
    opc_add_ini_default(ht, "opcache.max_accelerated_files",  "4000");
    opc_add_ini_default(ht, "opcache.protect_memory",          "0");    /* mprotect is a no-op here */
#else
    /* File cache on the microSD (default): the bytecode lives on the card and is reloaded per request
     * (skipping the recompile), so the request keeps the full PSRAM. The right choice for a large
     * framework. */
    opc_add_ini_default(ht, "opcache.file_cache",          s_opcache_dir);
    opc_add_ini_default(ht, "opcache.file_cache_only",     "1");
    opc_add_ini_default(ht, "opcache.max_accelerated_files", "20000");
#endif
}
#endif /* PHP_EXT_OPCACHE_ENABLED */

/*
 * The runtime bootstrap. Runs inside php_task (php_task.c), which has already line-buffered stdout
 * and logged its core. Returns true with the engine up and g_entry_script / g_src_dir published
 * (g_entry_script may be NULL when no script is present, in which case the caller runs the
 * engine-check fallback). Returns false only when the engine itself failed to start.
 */
bool boot_php_runtime(void)
{
    /* PHP allocations go to PSRAM (see CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0), which
     * keeps internal RAM free for DMA and FreeRTOS. */
    setenv("USE_ZEND_ALLOC", "0", 1);

#ifdef PHP_STORAGE_MICROSD
    /* microSD -- writable data storage, mounted whenever a card is present. Compiled out when
     * microSD support is off (-DPHP_STORAGE_MICROSD=OFF): a board without a card slot, or an
     * embedded project that didn't opt into the card. */
    s_have_sd = board_mount_storage(SD_MOUNT_POINT);
    if (s_have_sd) {
        ESP_LOGI(TAG, "microSD mounted at %s", SD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "microSD not mounted (no card / wrong format?)");
    }
#endif
    /* Embedded PHP source (read-only) -- only on firmware built for embedded storage. */
    bool have_app = mount_embedded();
    if (have_app) {
        ESP_LOGI(TAG, "embedded source mounted at %s", APP_MOUNT_POINT);
    }

#ifdef BOARD_HAS_NETWORK
    /* Boards with wired networking bring the link up here and log the address, so a PHP
     * script (e.g. a socket server) has the network ready and you can see where to reach
     * it. Non-fatal: without a cable/lease we just log it and carry on. */
    {
        char ip[16];
        if (board_network_up(ip, sizeof ip)) {
            snprintf(s_board_ip, sizeof s_board_ip, "%s", ip);
            ESP_LOGI(TAG, "network up -- http://%s/", ip);
        } else {
            ESP_LOGW(TAG, "network: no IP (link down or no DHCP)");
        }
    }
#ifdef PHP_NET_DNS
    /* Static DNS servers from [network] dns in the project config (","-separated, passed as
     * -DPHP_NET_DNS). Set them on the default netif *after* the DHCP lease so they take
     * precedence over DHCP-provided ones; if this list is empty the DHCP servers stand. */
    net_apply_static_dns(PHP_NET_DNS);
#endif
#endif

    /* Run the embedded source if it's there, otherwise the one on the card. */
    const char *script = NULL;
    const char *src_dir = NULL;   /* the mount that source lives on (for OPENSSL_CONF below) */
    if (have_app && access(APP_SCRIPT, R_OK) == 0) {
        script = APP_SCRIPT;
        src_dir = APP_MOUNT_POINT;
    }
#ifdef PHP_STORAGE_MICROSD
    else if (s_have_sd && access(SD_SCRIPT, R_OK) == 0) {
        script = SD_SCRIPT;
        src_dir = SD_MOUNT_POINT;
    }
#endif

    /* The full openssl build needs an openssl.cnf; point it at one shipped with the source
     * (see docs/openssl.md). The path is PHP_OPENSSL_CONF (set from the project config's
     * [extensions.openssl] config_path, default "openssl.cnf"): an absolute path is used as-is,
     * a relative one is resolved against the source mount. Harmless when openssl isn't built or
     * the file isn't there. */
#ifndef PHP_OPENSSL_CONF
#define PHP_OPENSSL_CONF "openssl.cnf"
#endif
    if (src_dir) {
        static char ossl_conf[128];
        if (PHP_OPENSSL_CONF[0] == '/')
            snprintf(ossl_conf, sizeof ossl_conf, "%s", PHP_OPENSSL_CONF);
        else
            snprintf(ossl_conf, sizeof ossl_conf, "%s/%s", src_dir, PHP_OPENSSL_CONF);
        setenv("OPENSSL_CONF", ossl_conf, 1);
    }

    /* The TLS client (PHP_EXT_OPENSSL_TLS) verifies peers against a CA bundle shipped with the
     * source. PHP_TLS_CAFILE is its path (from [extensions.openssl] certs_path, default
     * "certs/ca-bundle.crt"): absolute used as-is, relative resolved against the source mount. The
     * esp-tls factory reads $PHP_TLS_CAFILE; with none it connects unverified (and logs it). */
#ifdef PHP_TLS_CAFILE
    if (src_dir) {
        static char tls_ca[128];
        if (PHP_TLS_CAFILE[0] == '/')
            snprintf(tls_ca, sizeof tls_ca, "%s", PHP_TLS_CAFILE);
        else
            snprintf(tls_ca, sizeof tls_ca, "%s/%s", src_dir, PHP_TLS_CAFILE);
        setenv("PHP_TLS_CAFILE", tls_ca, 1);
    }
#endif

    php_embed_module.ub_write = esp_ub_write;

#ifdef PHP_PROJECT_WEB_SERVER
    /* Present a web-like SAPI name to the script. The embed SAPI is named "embed", which frameworks
     * treat as a CLI process -- Symfony then picks its CLI error renderer (which writes to
     * php://stdout, unavailable here) and Laravel's runningInConsole() returns true. In the
     * web-server model each run really is an HTTP request, so report "cli-server" (PHP's built-in
     * web server), which frameworks treat as web. Must be set before php_embed_init() so PHP_SAPI
     * reflects it. */
    php_embed_module.name = "cli-server";
#endif

#ifdef PHP_EXT_OPCACHE_ENABLED
    /* Enable OPcache: install the ini_defaults hook (must be set before php_embed_init reads the
     * ini). The mode is chosen at build time by the `in_memory` setting. */
#ifdef PHP_OPCACHE_SHM_MODE
    php_embed_module.ini_defaults = opcache_ini_defaults;   /* in-RAM (PSRAM) -- no card needed */
    ESP_LOGI(TAG, "opcache: in-RAM (PSRAM SHM) bytecode cache");
#elif defined(PHP_STORAGE_MICROSD)
    /* File cache: point it at a writable dir on the card. */
    if (s_have_sd) {
        snprintf(s_opcache_dir, sizeof s_opcache_dir, "%s/opcache", SD_MOUNT_POINT);
        mkdir(s_opcache_dir, 0777);   /* opcache requires the dir to exist; ok if it already does */
        php_embed_module.ini_defaults = opcache_ini_defaults;
        ESP_LOGI(TAG, "opcache: file cache at %s", s_opcache_dir);
    } else {
        ESP_LOGW(TAG, "opcache: no microSD, not enabled (needs a writable cache dir)");
    }
#endif
#endif

    /* Apply the baked .env before the engine starts, so it lands in $_ENV / getenv(). */
    apply_project_env();

    /* Fill the php-esp32 identity globals (surfaced in $_SERVER and phpinfo's "PHP Baremetal Infos"
     * table). NOT setenv'd, so they never enter $_ENV or the process environment. */
    php_esp32_board       = BOARD_NAME;
    php_esp32_version     = esp_app_get_description()->version;
    php_esp32_idf_version = esp_get_idf_version();
#ifdef PHP_ESP32_PROJECT_NAME
    php_esp32_project = PHP_ESP32_PROJECT_NAME;
#endif

    ESP_LOGI(TAG, "php_embed_init()...");
    if (php_embed_init(0, NULL) != SUCCESS) {
        ESP_LOGE(TAG, "php_embed_init failed");
        return false;
    }

    /* Register any per-project C extensions (from ./firmware/exts) before the script runs. */
    register_project_extensions();

    /* Firmware banners go straight to stdout, NOT through php_printf: php_printf runs PHP's output
     * layer, which marks SG(headers_sent), and that breaks anything the script does before its own
     * first output -- e.g. session_start() / ini_set('session...') refuse once headers are "sent".
     * esp_ub_write also just writes stdout, so the console output is identical either way. */
    printf("PHP %s on %s\n", PHP_VERSION, BOARD_SOC);
    fflush(stdout);

    /* Publish the entry + its source mount for the model runner (app.h). NULL entry => no script. */
    g_entry_script = script;
    g_src_dir      = src_dir;
    return true;
}

void boot_php_shutdown(void)
{
    php_embed_shutdown();

    if (s_app_mounted) {
        esp_vfs_fat_spiflash_unmount_ro(APP_MOUNT_POINT, "storage");
    }
#ifdef PHP_STORAGE_MICROSD
    if (s_have_sd) {
        board_unmount_storage(SD_MOUNT_POINT);
    }
#endif

    ESP_LOGI(TAG, "done -- heap free: %u bytes", (unsigned) esp_get_free_heap_size());
}

void app_main(void)
{
    ESP_LOGI(TAG, "starting PHP runtime");
#ifdef PHP_POWER_SAVE_ENABLED
    /* power_save: let FreeRTOS enter light sleep on its own when tasks block (the reactor idles there).
     * power_hold() in PHP takes a lock to forbid it during work. */
    esp_pm_config_t pm = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = 40,
        .light_sleep_enable = true,
    };
    if (esp_pm_configure(&pm) == ESP_OK) {
        ESP_LOGI(TAG, "power_save: esp_pm light sleep enabled (max %d MHz)", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    }
    bm_events_set_power_save(true);   /* let other components reject configs that fight light sleep */
#endif
    xTaskCreatePinnedToCore(php_task, "php", PHP_TASK_STACK_BYTES, NULL, 5, NULL, PHP_TASK_CORE);
}
