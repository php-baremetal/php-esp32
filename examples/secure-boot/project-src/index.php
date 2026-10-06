<?php
// A tiny app for the Secure Boot + Flash Encryption example. Nothing special in the code —
// the point is the firmware around it: only your signed, encrypted image boots on this chip.

function setup(): void
{
    printf("secure-boot demo — chip %s, %d MHz\n", sys_chip_model(), sys_cpu_freq_mhz());
    echo "if you can read this over serial, the signed image booted\n";
}

function loop(int $tick): void
{
    if ($tick % 5 === 0) {
        printf("alive, tick %d, uptime %d ms\n", $tick, sys_uptime_ms());
    }
    delay(1000);
}
