<?php
/*
 * server_init: runs once, at boot, before the HTTP server starts. Its output goes to the serial
 * console. Do one-time setup here whose effect lives below PHP and is shared by every request --
 * here we just seed the in-RAM mem_* store. (Hardware would be brought up via a C extension.)
 */
echo "[init] seeding mem_*\n";
mem_set('boot_msg', 'initialised once at boot');
mem_set('hits', 0);
