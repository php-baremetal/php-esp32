<?php
// ext/session on the microcontroller. The default "files" save handler persists $_SESSION to a
// writable directory -- here a folder on the microSD, which survives reboots. (With the web-server
// project type the same mechanism carries a session across HTTP requests.)

$dir = '/sdcard/sessions';
@mkdir($dir);
ini_set('session.save_path', $dir);
ini_set('session.use_cookies', '0');   // no HTTP here -- set the id ourselves

session_id('demo');
session_start();
$visits = ($_SESSION['visits'] ?? 0) + 1;
$_SESSION['visits'] = $visits;
$_SESSION['note']   = 'hello from PHP on ESP32';
session_write_close();
echo "session 'demo': visit #$visits (persisted to $dir)\n";

// Reopen the same session id and read it back.
session_start();
echo "read back -> visits=", $_SESSION['visits'], ", note=\"", $_SESSION['note'], "\"\n";
session_write_close();

echo "session file: ", (is_file("$dir/sess_demo") ? "present on the card" : "missing"), "\n";
echo "(reset the board and the visit count keeps climbing -- it lives on the card)\n";
