# imu-ws-stream

The whole 2.0.0 stack in one demo: the QMI8658 IMU — **accelerometer, gyroscope and temperature** —
**polled on core 1** streams to the browser **live over a WebSocket**, plotted in real time, and the
board serves the page itself. No `loop()`, no cloud, no router: one resident reactor on core 0 does
polling, the event bus, HTTP and WebSocket at once.

Accel and gyro come from the core-1 poll ring; the temperature (a separate register) is read a few times
a second and folded into the same stream. The page draws two scrolling plots (accel in g, gyro in °/s)
and a live temperature readout.

```
core 1:  poll QMI8658 (50 Hz) ───────────────→ ring buffer
core 0:  every(50ms) StreamTick ─ read latest from the ring ─ ws_broadcast(reading) ─→ every browser plots it
         reactor also serves GET / and /app.js over HTTP
```

## How it fits together

- **`$imu->poll(hz: 50, depth: 8)`** — the executor samples the IMU on core 1 into a ring buffer. No event
  per sample: that keeps the reactor queue free for the timer and the HTTP requests.
- **`every(50, StreamTick::class)`** — a fixed-rate broadcast clock. The handler reads the *latest* sample
  from the ring and `ws_broadcast()`s it as JSON — one predictable tick per frame (20 Hz), decoupled from
  the poll rate, so the stream stays smooth even under HTTP load.
- **`serve_ws('/ws') + serve_http(80)`** — the same server carries the page, its script, and the stream.
- **`app.js`** — connects to `/ws` and draws the X/Y/Z traces on a canvas as the frames arrive.

> Decoupling the broadcast (a 20 Hz timer sending the latest) from the poll (50 Hz filling the ring) is
> smoother than broadcasting once per sample: the reactor gets a steady tick instead of a per-sample
> burst that competes with the HTTP requests. For a purely reactive use (act on each new sample), poll
> with `event: SamplesReady::class` instead — see [`imu-poll-core1`](../imu-poll-core1/).

## Board

Any WiFi-capable ESP32-S3 with a **QMI8658** IMU on I²C (`SDA=11`, `SCL=10`). Adjust the pins in
`index.php` for your wiring.

## Run

```sh
phpflash flash
phpflash monitor      # prints "join 'php-imu' … open http://192.168.4.1/"
```

Join the **`php-imu`** WiFi network (password `baremetal`), open **http://192.168.4.1/**, and tilt the
board — the three accelerometer traces move in real time. Open it on a second device too: both plot the
same live stream (that's `ws_broadcast`).
