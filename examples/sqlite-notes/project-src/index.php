<?php
// PDO + SQLite, with the database file living on the microSD. The row written on
// each boot survives resets, because the .db file is on the card, not in RAM.
//
// This needs a firmware built WITH the sqlite extension (it's off by default):
// run ./flash.sh and answer "y" to sqlite, or build with idf.py -DPHP_EXT_SQLITE=ON.
//
// The DSN uses a file: URI with ?nolock=1 on purpose: FATFS has no POSIX file
// locking and this is a single-process device, so locking is neither available
// nor needed. The PRAGMAs keep writes friendly to a card with no fsync/journal
// story: the rollback journal is kept in RAM and commits don't fsync.

$dbfile = '/sdcard/notes.db';
$fresh  = !file_exists($dbfile);   // first run? the file isn't there yet

// PDO opens the file, creating it if it doesn't exist (SQLite's default).
$pdo = new PDO('sqlite:file:' . $dbfile . '?nolock=1');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->exec('PRAGMA journal_mode = MEMORY');
$pdo->exec('PRAGMA synchronous = OFF');

$pdo->exec('CREATE TABLE IF NOT EXISTS boots (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    note TEXT NOT NULL
)');

echo $fresh
    ? "created a new database at $dbfile\n"
    : "opened existing database $dbfile\n";

// record this boot
$pdo->prepare('INSERT INTO boots (note) VALUES (?)')
    ->execute(['booted PHP ' . PHP_VERSION]);

$count = $pdo->query('SELECT COUNT(*) FROM boots')->fetchColumn();
echo "boots recorded so far: $count\n";

echo "last 5:\n";
foreach ($pdo->query('SELECT id, note FROM boots ORDER BY id DESC LIMIT 5') as $row) {
    printf("  #%d  %s\n", $row['id'], $row['note']);
}
