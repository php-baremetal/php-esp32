<?php
// Standalone Eloquent -- Laravel's ORM (the illuminate/database package) with NO
// Laravel framework: just the Capsule manager, a Model, and a SQLite database file
// living on the microSD. A row is written on every boot and survives resets.
//
// This needs the "everything" firmware: sqlite + mbstring + ctype + filter + date
// all built in -- that's Eloquent's stack (PDO/SQLite for storage, the Str helpers,
// Carbon timestamps). See the README for the exact flash.sh answers / idf.py flags.
//
// Copy this folder to the microSD keeping the layout: index.php + the whole vendor/.

require __DIR__ . '/vendor/autoload.php';

use Illuminate\Database\Capsule\Manager as Capsule;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Schema\Blueprint;

// mbstring on this board is built without the oniguruma regex engine, so the
// mb_ereg*/mb_split family isn't available. Eloquent's Str helpers call mb_split()
// with simple patterns (e.g. '\s+'); back it with PCRE, which is always compiled in.
if (!function_exists('mb_split')) {
    function mb_split(string $pattern, string $string, int $limit = -1): array
    {
        $parts = preg_split('/' . $pattern . '/u', $string, $limit < 1 ? -1 : $limit);
        return $parts === false ? [$string] : $parts;
    }
}

date_default_timezone_set('UTC');   // Carbon timestamps, without needing named zones

$dbfile = '/sdcard/eloquent.db';
$fresh  = !file_exists($dbfile);

$capsule = new Capsule;
$capsule->addConnection(['driver' => 'sqlite', 'database' => $dbfile]);
$capsule->setAsGlobal();      // so Model::... resolves this connection
$capsule->bootEloquent();

// FATFS has no POSIX file locking, so hand Eloquent our own PDO opened with a
// file: URI and ?nolock=1 (needs SQLITE_USE_URI, on in this firmware). Injecting the
// PDO also bypasses the SQLite connector's realpath() check on a not-yet-created file.
$pdo = new PDO('sqlite:file:' . $dbfile . '?nolock=1');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
$pdo->exec('PRAGMA journal_mode = MEMORY');
$pdo->exec('PRAGMA synchronous  = OFF');
$capsule->getConnection()->setPdo($pdo);

// Schema, created on the first boot only -- via the Eloquent schema builder.
$schema = Capsule::schema();
if (! $schema->hasTable('posts')) {
    $schema->create('posts', function (Blueprint $table) {
        $table->increments('id');
        $table->string('title');
        $table->timestamps();
    });
}

// A model, the Eloquent way.
class Post extends Model
{
    protected $fillable = ['title'];
}

echo $fresh
    ? "created a new database at $dbfile\n"
    : "opened existing database $dbfile\n";

// Record this boot as a row (mass assignment + Carbon timestamps).
$post = Post::create(['title' => 'boot #' . (Post::count() + 1)]);
echo "inserted post #{$post->id} at {$post->created_at}\n";

// Read it back with the query builder.
echo "total posts: " . Post::count() . "\n";
echo "last 5:\n";
foreach (Post::orderBy('id', 'desc')->take(5)->get() as $p) {
    printf("  #%d  %-8s  %s\n", $p->id, $p->title, $p->created_at);
}
