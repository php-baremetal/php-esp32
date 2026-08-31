<?php
// A tiny database on the microcontroller via the SQLite3 class (ext/sqlite3), selected with
// [extensions.sqlite] type = "sqlite3". This is the counterpart to examples/sqlite-notes, which
// does the same thing through PDO -- the SQLite3 API is ~45 KB smaller because it skips the PDO
// layer. The .db lives on the microSD, so the boot counter survives resets.
//
// This needs a firmware built WITH the sqlite extension (off by default): the config's
// [extensions.sqlite] enabled + type = "sqlite3" turns it on and picks the SQLite3 API.
//
// The URI filename carries ?nolock=1 on purpose (SQLITE_USE_URI is compiled in): FATFS has no
// POSIX file locking and this is a single-process device, so locking is neither available nor
// needed. journal_mode=MEMORY + synchronous=OFF keep the journal in RAM and skip fsync.

$dbfile = '/sdcard/notes.db';
$fresh  = !file_exists($dbfile);   // first run? the file isn't there yet

// SQLite3 opens the file, creating it if missing.
$db = new SQLite3('file:' . $dbfile . '?nolock=1',
                  SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE);
$db->enableExceptions(true);
$db->exec('PRAGMA journal_mode = MEMORY');
$db->exec('PRAGMA synchronous = OFF');

$db->exec('CREATE TABLE IF NOT EXISTS boots (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    note TEXT NOT NULL
)');

echo $fresh
    ? "created a new database at $dbfile\n"
    : "opened existing database $dbfile\n";

// record this boot
$stmt = $db->prepare('INSERT INTO boots (note) VALUES (?)');
$stmt->bindValue(1, 'booted PHP ' . PHP_VERSION);
$stmt->execute();

$count = $db->querySingle('SELECT COUNT(*) FROM boots');
echo "boots recorded so far: $count\n";

echo "last 5:\n";
$res = $db->query('SELECT id, note FROM boots ORDER BY id DESC LIMIT 5');
while ($row = $res->fetchArray(SQLITE3_ASSOC)) {
    printf("  #%d  %s\n", $row['id'], $row['note']);
}
$db->close();
