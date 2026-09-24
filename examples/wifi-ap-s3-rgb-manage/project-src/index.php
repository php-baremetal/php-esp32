<?php
/**
 * index.php -- the view controller for the control page. init.php includes this with $state (the current
 * LED colour) in scope; it hands that colour to the page.php template, both as PHP values (server-rendered
 * initial slider positions) and as JSON for the page's WebSocket bootstrap.
 */

$color = $state;                 // the current LED colour {h, s, v, on}
$init  = json_encode($color);    // same, for window.__S in the page

include __DIR__ . '/page.php';
