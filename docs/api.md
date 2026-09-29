# DF_DMC_Common API

[← Index](../README.md)

Namespace `dfdmc`. Sources are in `src/`. A firmware depends on the sibling checkout:

```ini
build_flags =
    -DFDMC_MAX_AXES=6
lib_deps =
    symlink://../DF_DMC_Common
```

`DFDMC_MAX_AXES` sizes the path table (default 6). `DF_DMC_2_PWM` sets 16. Optional `DFDMC_MAX_UPLOAD_FRAMES` defaults to 2048.

## DMC framing — `dmc_protocol`

USB CDC frames: magic `DF`, little-endian, Fletcher-16. `DmcParser::feed` returns one `DmcFrame` per packet.

The header holds the message ids both boards use (`kDmcMsgHi`, motor move/stop/jog, GIO, DMX, realtime upload/run) and the ACK codes. `kDmxChannels` is 512.

Helpers: `appendByte` / `appendWordLE` / `appendDwordLE`, and the matching `read*` functions.

## Path upload — `PathTable`

Dragonframe realtime upload as int32 steps per frame, plus GIO triggers. SliderMC `PD` packing stays in the firmware that talks to a motion controller.

- `beginUpload(startFrame, endFrame, axisCount)` — `axisCount` may be 0
- `storeAxis` / `storeTrigger`
- `finishUpload` — marks the table ready when there is at least one frame
- `positionSteps`, `localFrame`, `triggerAtLocal`
- `empty()` is true until `finishUpload` succeeds

## DMX — `DmxEngine`

```text
begin(txPin, pwmPins, pwmCount, pwmHz, exponential)
```

Sends DMX on `txPin` (PIO UART, 250000 8N2, BREAK 176 µs, MAB 24 µs). A 150 µs timer repeats a frame about every 25 ms. The packet length is the highest channel set non-zero since `begin()`, and it does not shrink until the next power-up. Wire levels stay raw 0–255. Until that first non-zero set, the line stays in mark.

`pwmPins` / `pwmCount` mirror the first channels onto high-active PWM. `pwmHz` is 18000 on the current boards, wrap 254. `exponential` applies `level * level / 255` on the PWM pins only (0 = 0%, 128 ≈ 25%, 255 = 100%). Pass a null pin list and count 0 to send the universe with no mirror.

`apply(startChannel, levels, count, ramp)` writes the buffer. `ramp` fades over 500 ms. `update()` must be called every pass of the main loop: it advances a ramp and keeps the DMX stream running.

## GIO — `DmcGio`

```text
struct GioMap {
  outPins, outCount,
  inPins, inCount,
  cameraPin,   // 255 = none
  buzzerPin,   // 255 = none
  movePin      // 255 = none
};
```

`begin(map)` sets outputs and the camera pin to open-collector (active low, released = pull-up) and inputs to pull-up. A low input sets that bit. `pollInputChange` debounces and returns true once when the stable mask changes.

`pulseBuzzer(ms)` drives the buzzer pin high. `tick()` turns it off. `setMove` is not in this class: a firmware that has a MOVE pin drives it itself after `begin` has set the pin low.
