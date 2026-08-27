<?php
// wifi-ap-s3-rgb-manage :: the per-request handler behind the firmware's HTTP server (web-server
// model, shared-nothing like PHP under Apache). The board created its own WiFi network in init.php;
// connect to it and open http://192.168.4.1/.
//
//   GET /        -> the control page (hue / saturation / brightness, live)
//   GET /set?h=&s=&v=&on=  -> apply it to the LED, remember it in mem_*, return JSON
//
// State lives in the in-RAM mem_* store (seeded by init.php) so the sliders start where the LED is.

function clampi($val, int $lo, int $hi): int
{
    $val = (int) $val;
    return $val < $lo ? $lo : ($val > $hi ? $hi : $val);
}

$path = parse_url($_SERVER['REQUEST_URI'] ?? '/', PHP_URL_PATH);

if ($path === '/set') {
    // Params come in the query string ($_GET is populated from QUERY_STRING, the most reliable path
    // in this SAPI); fall back to $_POST, then to the stored value.
    $h  = clampi($_GET['h']  ?? $_POST['h']  ?? mem_get('h', 210), 0, 359);
    $s  = clampi($_GET['s']  ?? $_POST['s']  ?? mem_get('s', 255), 0, 255);
    $v  = clampi($_GET['v']  ?? $_POST['v']  ?? mem_get('v', 40),  0, 255);
    $on = clampi($_GET['on'] ?? $_POST['on'] ?? mem_get('on', 1),  0, 1);

    mem_set('h', $h);
    mem_set('s', $s);
    mem_set('v', $v);
    mem_set('on', $on);

    if ($on && s3_onboard_rgb_available()) {
        s3_onboard_rgb_hsv($h, $s, $v);
    } else {
        s3_onboard_rgb_off();
    }

    header('Content-Type: application/json');
    echo json_encode(['h' => $h, 's' => $s, 'v' => $v, 'on' => $on]);
    return;
}

// The control page. Read the current state so the controls open where the LED actually is.
$h  = (int) mem_get('h', 210);
$s  = (int) mem_get('s', 255);
$v  = (int) mem_get('v', 40);
$on = (int) mem_get('on', 1);

header('Content-Type: text/html; charset=utf-8');
?>
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-S3 RGB</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin: 0; font-family: system-ui, sans-serif; background: #14161a; color: #e8eaed;
         min-height: 100vh; display: flex; align-items: center; justify-content: center; }
  .card { width: min(92vw, 26rem); background: #1d2026; border: 1px solid #2c303a;
          border-radius: 1rem; padding: 1.5rem 1.5rem 1.75rem; box-shadow: 0 8px 30px #0006; }
  h1 { font-size: 1.15rem; margin: 0 0 1.25rem; font-weight: 600; letter-spacing: .01em; }
  h1 small { color: #8b93a1; font-weight: 400; }
  .swatch { height: 7rem; border-radius: .75rem; margin-bottom: 1.25rem; border: 1px solid #2c303a;
            transition: background .12s linear; }
  label { display: flex; justify-content: space-between; font-size: .82rem; color: #aab2c0;
          margin: 0 0 .35rem; }
  label b { color: #e8eaed; font-variant-numeric: tabular-nums; font-weight: 600; }
  input[type=range] { width: 100%; margin: 0 0 1.1rem; accent-color: #6ea8fe; }
  .row { display: flex; gap: .6rem; align-items: center; margin-top: .25rem; }
  button { flex: 1; padding: .7rem; border-radius: .6rem; border: 1px solid #2c303a;
           background: #262a33; color: #e8eaed; font-size: .9rem; cursor: pointer; }
  button.on { background: #244; border-color: #2f6f6f; }
  .live { justify-content: flex-start; gap: .5rem; align-items: center; margin: .9rem 0 0;
          font-size: .82rem; color: #aab2c0; cursor: pointer; }
  .live input { width: auto; margin: 0; accent-color: #6ea8fe; }
  .hint { margin: 1.1rem 0 0; font-size: .74rem; color: #6b7280; text-align: center; }
</style>
</head>
<body>
<div class="card">
  <h1>ESP32-S3 RGB <small>&mdash; live from PHP</small></h1>
  <div class="swatch" id="swatch"></div>

  <label>Hue <b><span id="hL"><?= $h ?></span>&deg;</b></label>
  <input type="range" id="h" min="0" max="359" value="<?= $h ?>">

  <label>Saturation <b><span id="sL"><?= $s ?></span></b></label>
  <input type="range" id="s" min="0" max="255" value="<?= $s ?>">

  <label>Brightness <b><span id="vL"><?= $v ?></span></b></label>
  <input type="range" id="v" min="0" max="255" value="<?= $v ?>">

  <div class="row">
    <button id="toggle" class="<?= $on ? 'on' : '' ?>"><?= $on ? 'LED is ON' : 'LED is OFF' ?></button>
  </div>
  <label class="live"><input type="checkbox" id="live" checked> update the LED while dragging</label>
  <p class="hint" id="status">served fresh by PHP <?= PHP_VERSION ?> on the chip &middot; no cloud, no router</p>
</div>

<script>
const $ = id => document.getElementById(id);
let on = <?= $on ? 'true' : 'false' ?>;

// HSV (h 0..359, s/v 0..255) -> CSS rgb, for an accurate preview swatch.
function hsv2rgb(h, s, v) {
  s /= 255; v /= 255;
  const c = v * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = v - c;
  let r = 0, g = 0, b = 0;
  if (h < 60)       { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else              { r = c; b = x; }
  const f = n => Math.round((n + m) * 255);
  return `rgb(${f(r)},${f(g)},${f(b)})`;
}

function paint() {
  const h = +$('h').value, s = +$('s').value, v = +$('v').value;
  $('hL').textContent = h; $('sL').textContent = s; $('vL').textContent = v;
  $('swatch').style.background = on ? hsv2rgb(h, s, v) : '#000';
}

let timer = null;
function push() {
  clearTimeout(timer);
  timer = setTimeout(() => {
    const q = new URLSearchParams({
      h: $('h').value, s: $('s').value, v: $('v').value, on: on ? 1 : 0
    });
    fetch('/set?' + q)
      .then(r => r.json())
      .then(d => {
        $('status').textContent =
          `LED ${d.on ? 'ON' : 'OFF'} — h=${d.h} s=${d.s} v=${d.v} (applied by the board)`;
      })
      .catch(e => { $('status').textContent = 'could not reach the board: ' + e; });
  }, 60);   // debounce: don't flood the chip while dragging
}

for (const id of ['h', 's', 'v']) {
  // While dragging (input): always repaint the on-screen preview; push to the LED only if "live".
  $(id).addEventListener('input',  () => { paint(); if ($('live').checked) push(); });
  // On release (change): always push -- so with "live" off the LED updates when you finish.
  $(id).addEventListener('change', () => { paint(); push(); });
}
$('toggle').addEventListener('click', () => {
  on = !on;
  $('toggle').classList.toggle('on', on);
  $('toggle').textContent = on ? 'LED is ON' : 'LED is OFF';
  paint(); push();
});

paint();
</script>
</body>
</html>
