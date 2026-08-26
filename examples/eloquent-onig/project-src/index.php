<?php
// Standalone Eloquent (illuminate/database, no Laravel) on a firmware where mbstring
// is built WITH oniguruma -- so mb_split() is native and no polyfill is needed. This
// is the same demo as ../eloquent-demo, minus the mb_split shim.
//
// Firmware: sqlite + mbstring (+ ONIG) + ctype + filter + date. In flash.sh answer
// "y" to mbstring and then "y" to the "mb_ereg*/mb_split regex" question; or build with
//   idf.py -DPHP_EXT_SQLITE=ON -DPHP_EXT_MBSTRING=ON -DPHP_EXT_MBSTRING_ONIG=ON \
//          -DPHP_EXT_CTYPE=ON -DPHP_EXT_FILTER=ON -DPHP_EXT_DATE=ON
//
// Copy this folder to the microSD keeping the layout: index.php + the whole vendor/.

require __DIR__ . '/vendor/autoload.php';

use Illuminate\Database\Capsule\Manager as Capsule;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Schema\Blueprint;

date_default_timezone_set('UTC');   // Carbon timestamps, without needing named zones

$dbfile = '/sdcard/eloquent-onig.db';
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

// mb_split is the native mbstring/oniguruma one here (no polyfill).
echo "mb_split available: " . (function_exists('mb_split') ? 'yes (native)' : 'no') . "\n";

// Record this boot as a row (mass assignment + Carbon timestamps).
$post = Post::create(['title' => 'boot #' . (Post::count() + 1)]);
echo "inserted post #{$post->id} at {$post->created_at}\n";

// Read it back with the query builder.
echo "total posts: " . Post::count() . "\n";
echo "last 5:\n";
foreach (Post::orderBy('id', 'desc')->take(5)->get() as $p) {
    printf("  #%d  %-8s  %s\n", $p->id, $p->title, $p->created_at);
}
