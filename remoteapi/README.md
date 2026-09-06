# Remote API

A local, in-process control API for AntScopeZ's currently-connected analyzer: device
enumeration, connect/disconnect, sweeps, and live point streaming. Meant for external
tools (scripts, an MCP server, a web dashboard) — the app's own GUI doesn't use it.

Off by default. Enable via Settings > General ("Enable Remote API" + port), or force it on
for one run with `-remote-api-port <n>`. Binds to `127.0.0.1` only unless reconfigured.
`-headless` skips showing the window (still a full GUI process, just not displayed).

## Wire protocol

Newline-delimited JSON (NDJSON) over a plain TCP socket — one JSON object per line, both
directions.

- Request: `{"cmd": "...", "id"?: <your token>, ...params}`
- Reply: `{"id": <echoed>, "ok": true|false, ...fields}` or `{"ok": false, "error": "..."}`
- Unsolicited push (streamed points, connect/disconnect broadcasts): `{"event": "...",
  ...fields}` — no `id`/`ok`, so it's always distinguishable from a reply on the same
  connection. A reply always precedes any event its own command caused; unrelated events
  (e.g. another connection's sweep) can arrive at any time.

## Commands

| Command | Behavior |
|---|---|
| `{"cmd":"status"}` | `{"connected":bool,"measuring":bool,"device":{"name":..,"serial":..}\|null}` |
| `{"cmd":"devices"}` | `{"devices":[{"type":"hid"\|"serial"\|"nano","name":..,"serial"?:..,"port"?:..}]}` -- currently connected HID/serial/NanoVNA hardware. BLE not included (async scan-then-callback, not a synchronous list). |
| `{"cmd":"connect","type":"hid"\|"serial"\|"nano","name":"...","port"?:"..."}` | Connect by type + model name (from `devices`' own `name` field). `port` optionally disambiguates serial devices. Immediate `{"connected":true}` once the local connect call succeeds; a later `{"event":"connected","device":{...}}` broadcast to every open connection once the real handshake completes. BLE is rejected explicitly (not yet supported). |
| `{"cmd":"disconnect"}` | Disconnects the current device; broadcasts `{"event":"disconnected"}`. |
| `{"cmd":"sweep","start_hz":<int>,"stop_hz":<int>,"points":<int>}` | Starts a sweep on the connected device. `{"ok":false,"error":"busy: ..."}` if a scan (from any source, including the GUI) is already running. Immediate `{"started":true}`; points stream to every subscribed connection as `{"event":"point",...}`, then `{"event":"sweep_done","count":N}`. |
| `{"cmd":"stop"}` | Stops the running sweep. `{"ok":false,"error":"not measuring"}` if idle. |
| `{"cmd":"subscribe","stream":"points"}` / `{"cmd":"unsubscribe",...}` | Toggles whether this connection receives `point`/`sweep_done` events -- independent of who started the sweep, so a connection can just watch. |
| `{"cmd":"last"}` | `{"points":[...]}` -- the most recently completed (or in-progress) sweep's points, for a client that connects after a scan finished. |

## Point shape

```json
{
  "freq_hz": 468000000.0,
  "impedance": {"r": 48.1, "x": 1.2},
  "swr": 1.05,
  "s11": {"re": 0.02, "im": 0.01},
  "s21": {"re": 0.9, "im": 0.0}
}
```

`s11`/`s21` are only present for devices that report complex S-parameters (NanoVNA);
HID/Serial devices only ever carry `freq_hz`/`impedance`/`swr`.

## Implementation

- `remoteapiserver.h/.cpp` — `RemoteApiServer`, owns the listening `QTcpServer`.
- `remoteapiconnection.h/.cpp` — `RemoteApiConnection`, one per socket: NDJSON framing,
  command dispatch, and the signal hookups into `AnalyzerPro` that drive both the
  connect/disconnect broadcasts and the point stream.
- `remoteapiprotocol.h` — shared string constants and the reflection-coefficient/
  impedance/SWR math shared by both the `RawData` (HID/Serial) and `SParamPoint`
  (NanoVNA) point-building paths.

Everything runs on the main Qt event loop — no separate thread. Reaches the analyzer via
`MainWindow::analyzer()` and a couple of small delegate methods
(`startRemoteSweep()`/`stopCurrentScan()`) rather than touching `Measurements` directly,
so a remote-triggered scan gets the same prep/cleanup the GUI's own Start/Stop paths rely
on.

**Not yet verified against real hardware** — built and tested against a running instance
with no analyzer attached (every error path, and the full command surface). Confirm
`connect`/`sweep`/point-streaming end to end once a device is available.
