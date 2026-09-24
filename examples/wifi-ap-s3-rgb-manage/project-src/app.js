// Control the board's onboard RGB LED over a WebSocket. window.__S (injected by the page) is the initial
// state. Three tabs:
//   Colour  -- manual sliders (sends {h,s,v,on}).
//   Effects -- effects that run ON THE BOARD (sends {effect:name}); they keep going with no browser open.
//   JS FX   -- effects generated HERE, in this file: the browser runs the animation and streams colours.

const S = window.__S;
const $ = id => document.getElementById(id);

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

// ---- WebSocket ----
let ws, sendTimer = null;
function connect() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen    = () => $('status').innerHTML = 'connected &middot; <b>live</b> from the chip';
  ws.onclose   = () => { $('status').innerHTML = 'disconnected &mdash; retrying&hellip;'; setTimeout(connect, 1000); };
  ws.onmessage = e => { Object.assign(S, JSON.parse(e.data)); render(); };
}
function wsSend(obj) { if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj)); }
function sendColour() {                                   // debounced while dragging
  clearTimeout(sendTimer);
  sendTimer = setTimeout(() => wsSend({ h: S.h, s: S.s, v: S.v, on: S.on }), 40);
}

// ---- JS-side effects: defined here, not on the board. Each returns the next {h,s,v} for frame n. ----
const JS_EFFECTS = {
  disco:   { label: 'Disco',   note: 'random colours', step: () => ({ h: (Math.random() * 360) | 0, s: 255, v: 60 }) },
  rainbow: { label: 'Rainbow', note: 'sweep the hue',  step: n  => ({ h: (n * 6) % 360, s: 255, v: 60 }) },
  pulse:   { label: 'Pulse',   note: 'breathe',        step: n  => ({ h: S.h, s: S.s, v: (8 + 60 * (0.5 - 0.5 * Math.cos(n / 7))) | 0 }) },
  strobe:  { label: 'Strobe',  note: 'hard on/off',    step: n  => ({ h: S.h, s: S.s, v: (n % 2) ? 0 : 80 }) },
  candle:  { label: 'Candle',  note: 'warm flicker',   step: () => ({ h: 25 + (Math.random() * 15 | 0), s: 230, v: 25 + (Math.random() * 35 | 0) }) },
};

let jsFx = 'none', jsN = 0, jsTimer = null;
function stopJsFx() { clearInterval(jsTimer); jsTimer = null; jsFx = 'none'; paintJsButtons(); }
function startJsFx(name) {
  wsSend({ effect: 'none' });                 // stop any board-side effect first
  stopJsFx();
  jsFx = name; jsN = 0; paintJsButtons();
  jsTimer = setInterval(() => {
    Object.assign(S, JS_EFFECTS[name].step(++jsN), { on: 1 });
    render();
    wsSend({ h: S.h, s: S.s, v: S.v, on: 1 });
  }, 120);
}

// Build the JS-FX tab buttons from JS_EFFECTS (generated here, not in PHP).
function buildJsButtons() {
  const box = $('fx-js');
  let html = '<button class="fx-btn" data-js="none">Stop<small>back to manual</small></button>';
  for (const [k, fx] of Object.entries(JS_EFFECTS)) {
    html += `<button class="fx-btn" data-js="${k}">${fx.label}<small>${fx.note}</small></button>`;
  }
  box.innerHTML = html;
  box.querySelectorAll('[data-js]').forEach(b => {
    b.addEventListener('click', () => (b.dataset.js === 'none' ? stopJsFx() : startJsFx(b.dataset.js)));
  });
}
function paintJsButtons() {
  document.querySelectorAll('#fx-js [data-js]').forEach(b =>
    b.classList.toggle('active', b.dataset.js === jsFx || (jsFx === 'none' && b.dataset.js === 'none')));
}

// ---- render the whole UI from S ----
function render() {
  $('hL').textContent = S.h; $('sL').textContent = S.s; $('vL').textContent = S.v;
  $('h').value = S.h; $('s').value = S.s; $('v').value = S.v;
  $('swatch').style.background = S.on ? hsv2rgb(S.h, S.s, S.v) : '#000';
  $('toggle').classList.toggle('on', !!S.on);
  $('toggle').textContent = S.on ? 'LED is ON' : 'LED is OFF';
  document.querySelectorAll('#tab-effects [data-fx]').forEach(b =>   // board-side effect highlight
    b.classList.toggle('active', b.dataset.fx === (S.effect || 'none')));
  paintJsButtons();
}

// ---- controls ----
for (const id of ['h', 's', 'v']) {
  $(id).addEventListener('input', () => { stopJsFx(); S[id] = +$(id).value; render(); sendColour(); });
}
$('toggle').addEventListener('click', () => { stopJsFx(); S.on = S.on ? 0 : 1; render(); sendColour(); });

// board-side effects (tab 2): tell the board to run it
document.querySelectorAll('#tab-effects [data-fx]').forEach(b =>
  b.addEventListener('click', () => { stopJsFx(); wsSend({ effect: b.dataset.fx }); }));

// tabs
document.querySelectorAll('.tab').forEach(t => t.addEventListener('click', () => {
  document.querySelectorAll('.tab').forEach(x => x.classList.toggle('active', x === t));
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  $('tab-' + t.dataset.tab).classList.add('active');
}));

buildJsButtons();
render();
connect();
