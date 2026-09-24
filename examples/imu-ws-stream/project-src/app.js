// Live plot of the QMI8658 from the board's WebSocket. Each frame is
// {ax,ay,az, gx,gy,gz, t} (g / dps / degC), pushed by the board at a steady ~20 Hz (a fixed-rate broadcast timer, decoupled from the core-1
// poll). No polling here -- we just draw what arrives.

const $ = id => document.getElementById(id);

// One scrolling plotter per canvas: three traces (x/y/z) at a fixed full-scale.
function plotter(canvasId, keys, cols, span) {
  const cv = $(canvasId), ctx = cv.getContext('2d');
  const W = cv.width, H = cv.height, MID = H / 2;
  const buf = { [keys[0]]: [], [keys[1]]: [], [keys[2]]: [] };
  function push(o) {
    for (const k of keys) { buf[k].push(o[k]); if (buf[k].length > W) buf[k].shift(); }
  }
  function draw() {
    ctx.clearRect(0, 0, W, H);
    ctx.strokeStyle = '#2c303a'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(0, MID); ctx.lineTo(W, MID); ctx.stroke();
    for (const k of keys) {
      const b = buf[k];
      ctx.strokeStyle = cols[k]; ctx.lineWidth = 1.5; ctx.beginPath();
      for (let i = 0; i < b.length; i++) {
        const x = W - b.length + i, y = MID - (b[i] / span) * MID;
        i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.stroke();
    }
  }
  return { push, draw };
}

const accel = plotter('plotA', ['ax', 'ay', 'az'], { ax: '#ff6b6b', ay: '#51cf66', az: '#6ea8fe' }, 2.5);
const gyro  = plotter('plotG', ['gx', 'gy', 'gz'], { gx: '#ffa94d', gy: '#da77f2', gz: '#22b8cf' }, 550);

let frames = 0;
setInterval(() => { $('rate').textContent = frames + ' Hz'; frames = 0; }, 1000);

function onFrame(o) {
  accel.push(o); gyro.push(o);
  $('ax').textContent = o.ax.toFixed(2); $('ay').textContent = o.ay.toFixed(2); $('az').textContent = o.az.toFixed(2);
  $('gx').textContent = o.gx.toFixed(0); $('gy').textContent = o.gy.toFixed(0); $('gz').textContent = o.gz.toFixed(0);
  if (o.t !== undefined) $('t').textContent = o.t.toFixed(1);
  frames++;
}

function loop() { accel.draw(); gyro.draw(); requestAnimationFrame(loop); }

let ws;
function connect() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen    = () => $('status').innerHTML = '<b>live</b> &mdash; polled on core 1, streamed from PHP';
  ws.onclose   = () => { $('status').innerHTML = 'disconnected &mdash; retrying&hellip;'; setTimeout(connect, 1000); };
  ws.onmessage = e => { try { onFrame(JSON.parse(e.data)); } catch (_) {} };
}

connect();
requestAnimationFrame(loop);
