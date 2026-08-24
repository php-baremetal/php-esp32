<?php
/**
 * env-demo: the device configuration comes from the project's .env, compiled into the firmware and
 * read here as $_ENV / getenv(). Nothing is hardcoded in this script and nothing lives on a microSD
 * -- change .env, rebuild, and the values change. See docs/environment.md.
 */

/** An env value with a fallback (env vars are always strings). */
function env(string $key, string $default = ''): string
{
    return $_ENV[$key] ?? $default;
}

function setup(): void
{
    echo "\n=== env-demo :: configuration from .env ===\n";
    printf("  device name : %s\n", env('DEVICE_NAME', '(unset)'));
    printf("  API base    : %s\n", env('API_BASE', '(unset)'));
    printf("  sample rate : %d Hz  (cast from the \"%s\" string)\n",
        (int) env('SAMPLE_HZ', '1'), env('SAMPLE_HZ', '1'));
    printf("  debug       : %s\n", env('DEBUG', '0') === '1' ? 'on' : 'off');
    printf("  greeting    : %s  (via getenv())\n", getenv('GREETING') ?: '(unset)');
    echo "  edit .env and re-run `phpflash build` to change these\n";
    echo "===========================================\n";
}

function loop(int $tick): void
{
    // DEBUG arrives as the string "1"/"0"; treat it as a flag.
    if (env('DEBUG', '0') === '1') {
        printf("[%s] heartbeat #%d\n", env('DEVICE_NAME', 'device'), $tick);
    }
    delay(2000);
}
