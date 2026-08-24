<?php
/**
 * store-demo: a boot counter that survives resets, kept in the reboot-persistent store (NVS).
 * Reset the board a few times and watch the count climb. See docs/store.md.
 */
function setup(): void
{
    echo "\n=== store-demo :: persistent boot counter ===\n";
    if (!store_available()) {
        echo "  persistence is OFF -- add [store] size_kb to php-esp32.config.toml\n";
        echo "=============================================\n";
        return;
    }

    $boots = (int) store_get('boots', '0') + 1;   // values are strings; cast to use them
    store_set('boots', (string) $boots);
    if ($boots === 1) {
        store_set('first_msg', 'hello from boot #1');
    }

    printf("  boot count : %d   (survives resets)\n", $boots);
    printf("  first_msg  : %s\n", store_get('first_msg', '(none)'));
    printf("  keys       : %s\n", implode(', ', store_keys()));
    echo "  reset the board -- the count keeps climbing\n";
    echo "=============================================\n";
}

function loop(int $tick): void
{
    delay(5000);
}
