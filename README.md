# DF_DMC_Common

Shared Dragonframe **DMC v2** pieces for Pico firmwares. [DF_DMC_2_MC](https://github.com/fablab-wue/DF_DMC_2_MC) and [DF_DMC_2_PWM](https://github.com/fablab-wue/DF_DMC_2_PWM) both use this library. Later boards with a Dragonframe DMC interface should use it too.

**Documentation:** [API](docs/api.md) · [Dragonframe connect](docs/dragonframe.md) · [Go motion](docs/dragonframe.md#go-motion)

## What is in here

- **DMC v2 framing** — USB CDC parser, Fletcher-16 checksum, message ids
- **Path upload table** — int32 steps per frame, plus GIO triggers. SliderMC `PD` packing stays in the firmware that talks to a motion controller
- **DMX512** — break / mark-after-break / PIO UART, plus an optional PWM mirror of the first channels. Levels on the wire stay raw 0–255
- **GIO** — open-collector outputs, pull-up inputs, camera shutter, buzzer. Pin 255 means that function is absent

## Use it from a firmware

Clone this repo next to the firmware and point PlatformIO at the sibling folder:

```ini
build_flags =
    -DDFDMC_MAX_AXES=6
lib_deps =
    symlink://../DF_DMC_Common
```

`DFDMC_MAX_AXES` sizes the path table (default 6). `DF_DMC_2_PWM` sets it to 16. Optional `DFDMC_MAX_UPLOAD_FRAMES` defaults to 2048.

Sources live in `src/` under the `dfdmc` namespace: `dmc_protocol`, `path_table`, `dmx_engine`, `gio_io`.

## License

Copyright (c) 2026 Jochen Krapf \<jk@nerd2nerd.org\>

Licensed under the [MIT License](LICENSE).
