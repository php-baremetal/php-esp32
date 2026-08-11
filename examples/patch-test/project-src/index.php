<?php
/*
 * Patch regression test for the php-esp32 port.
 *
 * Exercises the runtime behaviour each vendored patch enables or fixes, so a PHP version
 * bump can be checked at a glance: build this, flash it, read the serial log. Every check
 * prints PASS, FAIL, or SKIP (when its extension or a resource such as a microSD is absent).
 *
 * Patch coverage:
 *   0001 closure run-time-cache arena     -> the closures section (real path only when opcache is OFF)
 *   0002 ext/date optional minimal tz     -> the date section (the full-tz-db path)
 *   0003 mbstring optional no-CJK         -> the mbstring section (the CJK-codecs-present path)
 *   0004 csprng esp getrandom             -> the random section
 *   0005 session files no-cloexec warn    -> the session section (needs a writable microSD)
 *   0006 opcache static embed             -> the opcache section (only when opcache is enabled)
 *   0007 opcache malloc SHM backend       -> the opcache section (only with in_memory opcache)
 */

$pass = 0; $fail = 0; $skip = 0;
function ok(string $name, bool $cond): void {
    global $pass, $fail;
    if ($cond) { $pass++; echo "  [PASS] $name\n"; }
    else       { $fail++; echo "  [FAIL] $name\n"; }
}
function skip(string $name, string $why): void {
    global $skip; $skip++; echo "  [SKIP] $name ($why)\n";
}

echo "==== php-esp32 patch test suite ====\n";
echo "PHP version : " . PHP_VERSION . "\n";
echo "extensions  : " . implode(', ', get_loaded_extensions()) . "\n\n";

/* ---- 0001: closures run-time-cache arena ------------------------------------------------
 * The fix allocates a scope-bound closure's non-shared run-time cache from the request arena
 * instead of emalloc()/efree(), which corrupted the PSRAM heap. Frameworks lean on
 * scope-bound closures heavily. This only takes the patched path when OPcache is OFF; with
 * OPcache on, the cache comes from its shared memory instead. Either way these must be correct. */
echo "-- 0001 closures run-time-cache arena --\n";
class Counter {
    public int $n = 0;
    public function adder(): \Closure {
        return function (int $by): int { $this->n += $by; return $this->n; };   // captures $this
    }
}
$c = new Counter();
$add = $c->adder();
ok("scope-bound closure (\$this)", $add(5) === 5 && $add(3) === 8 && $c->n === 8);

$bound = \Closure::bind(function (int $x): int { return $x * $this->k; }, new class { public int $k = 3; }, null);
ok("Closure::bind", $bound(4) === 12);

$total = 0;
for ($i = 0; $i < 3000; $i++) {           // many closures: each gets an arena run-time cache
    $f = fn(int $x): int => $x * 2;
    $total += $f($i);
}
ok("3000 closures, no heap corruption", $total === 2 * (2999 * 3000 / 2));

/* ---- 0002: ext/date full timezone database ---------------------------------------------
 * The patch makes the timezone DB swappable (full vs UTC-only). This build uses the full DB,
 * so named zones and their DST rules must resolve. */
echo "-- 0002 ext/date timezone db --\n";
if (class_exists('DateTime')) {
    $d = new DateTime('2026-07-30 12:00:00', new DateTimeZone('UTC'));
    $d->setTimezone(new DateTimeZone('Europe/Rome'));                 // +02:00 in July (DST)
    ok("named zone Europe/Rome (DST +02:00)", $d->format('H:i P') === '14:00 +02:00');
    $diff = (new DateTime('2026-03-01'))->diff(new DateTime('2026-07-15'));
    ok("date interval math", $diff->m === 4 && $diff->days === 136);
} else {
    skip("DateTime", "date extension off");
}

/* ---- 0003: mbstring with the CJK codecs ------------------------------------------------
 * The patch makes the legacy CJK codecs optional. This build keeps them, so a UTF-8 to
 * Shift-JIS round trip and mb_convert_kana must work; oniguruma (mb_ereg) is on too. */
echo "-- 0003 mbstring CJK codecs --\n";
if (function_exists('mb_strlen')) {
    ok("mb_strlen multibyte", mb_strlen('àèìòù', 'UTF-8') === 5 && mb_strlen('日本語', 'UTF-8') === 3);
    $sjis = mb_convert_encoding('日本語', 'SJIS', 'UTF-8');
    ok("UTF-8 <-> SJIS round trip (CJK codec)", $sjis !== '' && mb_convert_encoding($sjis, 'UTF-8', 'SJIS') === '日本語');
    ok("mb_convert_kana present", function_exists('mb_convert_kana'));
    if (function_exists('mb_ereg')) {
        ok("mb_ereg (oniguruma)", (bool) mb_ereg('^[0-9]+$', '12345'));
    } else {
        skip("mb_ereg", "onig off");
    }
} else {
    skip("mbstring", "mbstring off");
}

/* ---- 0004: CSPRNG via the hardware getrandom -------------------------------------------
 * Without the patch there is no /dev/urandom and getrandom is not wired up, so random_int
 * and random_bytes throw. The patch routes them to esp_fill_random (the hardware RNG). */
echo "-- 0004 csprng (hardware getrandom) --\n";
try {
    $a = random_int(1, 1_000_000);
    $b = random_int(1, 1_000_000);
    ok("random_int returns an int", is_int($a) && is_int($b));
    $r1 = random_bytes(32);
    $r2 = random_bytes(32);
    ok("random_bytes(32) length", strlen($r1) === 32 && strlen($r2) === 32);
    ok("output has entropy (differs)", $r1 !== $r2 && $a !== $b);
} catch (\Throwable $e) {
    ok("csprng threw: " . $e->getMessage(), false);
}

/* ---- 0006 / 0007: OPcache -------------------------------------------------------------
 * 0006 links OPcache statically and accepts the embed SAPI, so it registers and enables.
 * 0007 gives it a PSRAM-backed shared-memory segment, used in the in_memory cache mode.
 * Only present when the project enables opcache. */
echo "-- 0006/0007 OPcache --\n";
if (function_exists('opcache_get_status')) {
    $st = opcache_get_status(false);
    ok("0006 OPcache loaded and enabled", is_array($st) && !empty($st['opcache_enabled']));
    if (is_array($st) && isset($st['memory_usage']['used_memory'])) {
        ok("0007 in-memory SHM backend (used_memory > 0)", $st['memory_usage']['used_memory'] > 0);
    } else {
        skip("0007 SHM backend", "not in in_memory mode");
    }
} else {
    skip("0006/0007 OPcache", "opcache off in this build; enable [extensions.opcache] in_memory to test");
}

/* ---- 0005: session files save handler --------------------------------------------------
 * The files handler used to print a bogus fcntl(F_SETFD) warning on every session_start(),
 * because the FATFS VFS rejects F_SETFD. The patch attempts it but does not warn. Needs a
 * writable microSD for the files handler. */
echo "-- 0005 session files handler --\n";
if (function_exists('session_start')) {
    if (@is_dir('/sdcard') && @is_writable('/sdcard')) {
        $dir = '/sdcard/patch-test-sess';
        @mkdir($dir, 0777, true);
        ini_set('session.save_handler', 'files');
        ini_set('session.save_path', $dir);
        $warned = null;
        set_error_handler(function ($no, $msg) use (&$warned) {
            if (stripos($msg, 'F_SETFD') !== false || stripos($msg, 'fcntl') !== false) { $warned = $msg; }
            return true;
        });
        session_start();
        $_SESSION['probe'] = 4242;
        $id = session_id();
        session_write_close();

        session_id($id);
        session_start();
        $roundtrip = $_SESSION['probe'] ?? null;
        session_write_close();
        restore_error_handler();

        ok("session persists via the files handler", $roundtrip === 4242);
        ok("no bogus fcntl/F_SETFD warning", $warned === null);
    } else {
        skip("session files handler", "/sdcard not writable (no card?)");
    }
} else {
    skip("session", "session off");
}

echo "\n==== RESULT: $pass passed, $fail failed, $skip skipped ====\n";
