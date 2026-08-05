<?php

use Illuminate\Support\Facades\Route;

// Route::view (not a closure) so the route table can be cached with `php artisan route:cache`.
// A closure route can't be serialized, which disables route caching for the whole app.
Route::view('/', 'welcome');
