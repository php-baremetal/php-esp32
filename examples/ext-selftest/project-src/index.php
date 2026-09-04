<?php
// A self-test for the native extensions: it exercises each one on the board and prints PASS/FAIL,
// so a regression in the extension layer shows up on the serial log. It checks the shape of the
// 1.0 API -- every extension's _available() probe, typed return values, argument coercion, the
// sys_delay()/delay() pair, and the deprecated psram_*/heap_* aliases of the sys_* memory functions.
//
// It adapts to what's compiled in: the opt-in extensions (wifi, s3_onboard_rgb) are guarded with
// function_exists(), so the script still runs on a build with fewer of them.

$pass = 0; $fail = 0; $deprecations = [];
set_error_handler(function ($errno, $msg) use (&$deprecations) {
    if ($errno === E_DEPRECATED) { $deprecations[] = $msg; return true; }
    return false;
});
function check(string $label, bool $cond): void {
    echo ($cond ? "  PASS  " : "  FAIL  ") . "$label\n";
    $cond ? $GLOBALS['pass']++ : $GLOBALS['fail']++;
}

echo "==== ext-selftest (PHP " . PHP_VERSION . ") ====\n";

// 1) every native extension exposes an _available() probe that returns a bool
check('gpio_available() bool',  is_bool(gpio_available()));
check('mem_available() bool',   is_bool(mem_available()));
check('store_available() bool', is_bool(store_available()));
check('sys_available() bool',    is_bool(sys_available()));
if (function_exists('wifi_available'))           check('wifi_available() bool', is_bool(wifi_available()));
if (function_exists('s3_onboard_rgb_available')) check('s3_onboard_rgb_available() bool', is_bool(s3_onboard_rgb_available()));

// 2) sys timing + info; delay() is a plain alias of sys_delay() (no deprecation warning)
sys_delay(1); check('sys_delay(1) ran', true);
$b = count($deprecations); delay(1);
check('delay() is a clean alias (no E_DEPRECATED)', count($deprecations) === $b);
check('sys_uptime_ms() > 0', sys_uptime_ms() > 0);
check('sys_chip_model() is string', is_string(sys_chip_model()) && sys_chip_model() !== '');
check('sys_mac() looks like a MAC', (bool) preg_match('/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/', sys_mac()));
check('sys_idf_version() is string', is_string(sys_idf_version()) && sys_idf_version() !== '');

// 3) sys_* are canonical for memory; the unprefixed psram_*/heap_* names still work but are deprecated
check('sys_psram_free() > 0', sys_psram_free() > 0);
check('sys_heap_free() > 0',  sys_heap_free() > 0);
$b = count($deprecations); $v = psram_free();
check('psram_free() still returns int', is_int($v) && $v > 0);
check('psram_free() emits E_DEPRECATED', count($deprecations) > $b);

// 4) typed return values behave as declared
check('mem_keys() array', is_array(mem_keys()));
mem_set('answer', 42);
check('mem_get() round-trip (mixed)', mem_get('answer') === 42);
check('mem_get() default', mem_get('nope', 'def') === 'def');
if (function_exists('wifi_ip')) { $ip = wifi_ip(); check('wifi_ip() is ?string', $ip === null || is_string($ip)); }
if (store_available()) { store_set('t', 'hi'); check('store round-trip', store_get('t') === 'hi'); }

// 5) argument type coercion (weak mode): a numeric string coerces to int
gpio_mode(2, GPIO_OUTPUT);
gpio_write(2, "1");
check('gpio_write coerces "1" -> 1', gpio_read(2) === 1);
if (function_exists('s3_onboard_rgb_set')) { s3_onboard_rgb_set(0, 8, 0); check('s3_onboard_rgb_set ran', true); }

echo "\n==== $pass passed, $fail failed ====\n";
if ($deprecations) {
    echo "deprecations (expected for psram_/heap_):\n";
    foreach (array_unique($deprecations) as $d) echo "  - $d\n";
}
echo "SELFTEST DONE\n";
