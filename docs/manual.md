# DF_DMC user manual

[← Index](../README.md)

This is the user manual for the three Dragonframe boards that share this library:

- [DF_DMC_2_MC](https://github.com/fablab-wue/DF_DMC_2_MC) — RP2040-Zero in front of a SliderMC motion controller
- [DF_DMC_2_PWM](https://github.com/fablab-wue/DF_DMC_2_PWM) — Raspberry Pi Pico, hobby servos
- [DF_DMC_2_MKS](https://github.com/fablab-wue/DF_DMC_2_MKS) — RP2040-Zero, eight MKS closed-loop steppers and eight hobby servos

Dragonframe’s own books are linked below. This page does not copy them. It says how these boards behave, and which Dragonframe chapter covers the same idea on the PC.

Current user manual: [Using Dragonframe 2025](https://www.dragonframe.com/download/Using%20Dragonframe%202025.pdf). Older editions: [2024](https://www.dragonframe.com/download/Using%20Dragonframe%202024.pdf), [5](https://www.dragonframe.com/download/Using%20Dragonframe%205.pdf). Wire format: [DMC v2 protocol (2024-08-13)](https://www.dragonframe.com/download/dmcproto/DMC-Protocol-2024-08-13.pdf). How Dragonframe expects a third-party controller: [integrate a motion-control system](https://www.dragonframe.com/ufaqs/how-do-i-integrate-a-motion-control-system-with-dragonframe/). Hardware Dragonframe ships: [compatible hardware](https://www.dragonframe.com/ufaqs/what-hardware-is-compatible-with-dragonframe-motion-control-workspace/). The dmc-lite sketch that ships with Dragonframe 2024 and newer is on disk at `Resources/Arc Motion Control/dmc`, not on the web. General lessons: [Dragonframe tutorials](https://www.dragonframe.com/tutorials/).

Go-motion payload detail stays in [dragonframe.md](dragonframe.md#go-motion).

## Overview

Each board is a USB device. Dragonframe talks binary **DMC v2** on the COM port. Device type in Scene → Connections is **dmc-lite**. One connection can expose at most 16 motors.

| Board | Computer sees | Motion |
|-------|----------------|--------|
| DF_DMC_2_MC | `jDF-MC V1 …` | UART to SliderMC. Axes are whatever `CG` reports, 1–6. |
| DF_DMC_2_PWM | `jDF-PWM V1 …` | Hobby-servo PWM. DIP chooses 0, 2, 4, 6, 8, 10, 12, or 16 axes. Leftover PWM pins can mirror DMX. |
| DF_DMC_2_MKS | `jDF-MKS V1 8M+8S+…` | Motors 1–8 are MKS SERVO42/57D on one RS485 bus. Motors 9–16 are hobby servos on GP1–GP8. |

Arc is Dragonframe’s motion workspace. Open it from the top of the window. Chapter 16, Motion Control, page 269. Real-time moves with a DMC device: page 316.

The main animation play button plays captured frames. It does not send a motor position. Scrubbing and playing a move in Arc does.

## Connect

1. Close anything else that has the board’s COM port open (PlatformIO monitor, a terminal).
2. In Dragonframe: **Scene → Connections → Add connection**.
3. Device type **dmc-lite**.
4. Select this board’s serial port.
5. **Connect**.

The board sends a hello when the port opens, and answers Dragonframe’s `MSG_HI`. USB CDC is binary DMC. The serial monitor is not a text console.

### Steps per unit

DMC positions are signed integers. Arc’s **steps per unit** turns those integers into the numbers you type.

| Board | In Arc |
|-------|--------|
| DF_DMC_2_MC | **1000**. 1000 steps = 1 mm or 1 degree. Same scale for speed and acceleration. |
| DF_DMC_2_PWM | **1**, and type step counts. Do not use 1000. A servo keyframe of 0 and 15000 is most of the horn travel. |
| DF_DMC_2_MKS motors 1–8 | One step is one encoder count. **16384** counts is one motor revolution. Set steps per unit from the mechanics (leadscrew, pulley, gearbox). |
| DF_DMC_2_MKS motors 9–16 | Same pulse scale as the PWM board. Steps per unit **1**, keyframes **0** and **15000**, to see the horn move. |

Enable the axis in Arc before you expect the horn or the shaft to hold. A cleared enable flag limps a PWM servo (pulse width 0) and drops enable on an MKS drive.

## Features

Hello capabilities and what the bridge actually does:

| | MC | PWM | MKS |
|--|----|-----|-----|
| Real-time path (`0x0001`) | yes | yes | yes |
| Go motion `SHOOT_FRAME` (`0x0002`) | yes | yes | yes |
| Go motion `SHOOT_FRAME2` (`0x0080`) | yes | yes | yes |
| Real-time camera (`0x0400`) | yes | yes | yes |
| Live DMX `0x0020` | 512 channels | 512 channels | 512 channels |
| Timeline DMX with the move | 32 channels | 32 channels | 32 channels |
| Upload frames | 1440 | 1440 | 1440 |
| GIO out / in | 4 / 4 | 2 / 1 | 4 / 2 |
| Camera shutter | GP9, and SliderMC `CT` | GP17 | GP9 |
| Buzzer | GP10, and SliderMC `BE` | GP22 | GP10 |

Shared behavior:

- **Upload** (`0x0100`–`0x0104`) stores the move on the board: one step position per axis per frame, GIO output bits on frames that have triggers, and up to 32 DMX channels.
- **Playhead.** Dragging the Arc playhead sends `MSG_RT_POSITION_FRAME` (`0x0110`). The board moves every axis to that frame and, when a lighting track was uploaded, sets those DMX channels.
- **Play.** `MSG_RT_RUN_MOVE` (`0x0111`) moves to the preroll pose and waits. That pose is half a preroll interval before the start frame, the point a rest-to-cruise ramp leaves so it reaches the start frame at speed. PWM and MKS then step from that frame through the postroll pose. MC moves to the preroll pose, plays the uploaded range on SliderMC, then moves to the postroll pose. If the preroll or postroll pose would pass a soft limit, the board answers preroll (`0x0017`) or postroll (`0x0018`). A frame inside the played range that would pass a soft limit is rejected with the soft-limit code. `MSG_RT_GO` (`0x0113`) starts the clock only after the axes are idle. If they are still moving, the board answers not-in-position (`0x0016`). Each new frame sends a position report whose time field is thousandths of a frame (frame 2 is 2000). When the move asked to sync DMX, the stored lighting for that frame goes out with it. Video (`0x10`) holds the camera shutter open for the move. Stills (`0x20`) open it on each frame between the shutter angles. `MSG_RT_END` (`0x0114`) is sent when playback finishes and when a go-motion exposure finishes or is aborted.
- **Live DMX** (`0x0020`) writes channel levels now. Ramp follows the message flag. This is independent of the stored lighting track. Chapter 14, Automate Lighting with DMX, page 223. Connecting a universe: page 227. Previewing a program: page 245. Dragonframe’s own realtime lighting on a DMC-32 plays a full universe from the controller. These boards play the uploaded track for **32 channels**. A 33rd channel is rejected (`0x0014`).
- **Jog.** The speed word is 1–10000. 10000 is the axis max from `MOTOR_SET_SPEED`.
- **Stop.** Stop-all and hard-stop halt every axis. A second stop-all within about 500 ms hard-stops. PWM cuts the slew. MKS sends the drive hard-stop. MC sends SliderMC `MS` again, which is its only stop. Stop-one on MC and MKS halts that axis. PWM stop-one cuts the whole servo bank. Stopping during a go-motion shoot sends `MSG_RT_END`.

All three boards advertise go motion. PWM blur is a real pulse trapezoid. MKS blur is one linear RPM move. MC runs the blur as a timed SliderMC move. The shutter follows Dragonframe’s clock, and the board sends `MSG_RT_END` when the exposure finishes. See [Go motion](dragonframe.md#go-motion). Dragonframe’s go-motion chapter for an external rig is Chapter 15, page 265.

## Limits

- **16 motors** on one DMC connection. MC is further limited to the 6 axes SliderMC reports.
- **1440 frames** in one upload. That is 60 seconds at 24 fps. At a higher scene rate the same 1440 frames cover less wall-clock time. Dragonframe will not upload more than the hello field `UPLOAD FRAME COUNT`.
- **32 DMX channels** stored with the move, chosen from the channels the upload actually uses, not forced to 1–32. Live output is still a 512-channel universe.
- **Servo pulse** about −20000..+20000 counts around a 1.5 ms center (about 0.5 ms to 2.5 ms). Values outside that are clamped. A curve of 0–90 does not leave a hobby servo’s deadband.
- **MKS acceleration** is a linear RPM staircase. The drive’s accel code is 0–255. A one-second Dragonframe ramp at low RPM will not match the screen. Slow moves have short ramps. Eight drives on one RS485 bus can lag a 24 fps slice; servo and DMX frames still advance on the clock.
- **MKS homing.** There is no DMC home command. Jog to the mark, then `MOTOR_RESET_POSITION` (`0x0035`). That zeroes the reported count. It does not run the drive’s own go-home.
- **Soft limits.** A point move or jog outside an enabled limit is rejected: lower `0x0021`, upper `0x0020`. A preroll or postroll pose past an enabled limit is rejected with `0x0017` or `0x0018`. A frame of the played range past a limit is rejected with the soft-limit code. The hardware limit-switch flag in `MOTOR_SET_LIMITS` is ignored until a switch is wired.
- **No virtual rigs, no coupled motors, no ping-pong loop.** Those capability bits are not set.

## GIO inputs and outputs

These are Dragonframe’s general-purpose triggers. They are not the camera shutter and not the buzzer. How to add a logic output or a switch in the scene is Chapter 15, Adding Input and Output Triggers, page 259.

**GIO OUT** (`MSG_GIO_OUT`, `0x0021`). One bit per output. Bit 0 is the first pin in that board’s list. A set bit turns the pin **on**: open-collector, driven **low**. A clear bit releases the pin to its pull-up, so the wire sits **high**. The same bits can be stored on frames of the move (`0x0104`) and are applied when that frame is shown. They are not PWM and not DMX.

**GIO IN** (`MSG_GIO_IN`, `0x0022`). The pin is a pull-up. A switch to ground reads as that bit **set** (active **low**). The board answers a poll, and also sends an unsolicited input message after the level has stayed the same for about 20 ms.

| Board | Outputs | Inputs |
|-------|---------|--------|
| MC | OUT0–OUT3 = GP1, GP2, GP3, GP4 | IN0–IN3 = GP5, GP6, GP7, GP8 |
| PWM | OUT0–OUT1 = GP26, GP27 | IN0 = GP28 |
| MKS | OUT0–OUT3 = GP29, GP28, GP27, GP26 | IN0 = GP15, IN1 = GP14 |

Wire a lamp or relay from the output pin to the load, with the other side of the load to the board’s positive rail only if that rail matches the part. The pin sinks to ground when the bit is on. Do not feed 5 V into an input. A dry contact to GND is the intended switch.

## Pins

Electrical rules that are the same on every board:

- GIO outputs and the camera shutter are open-collector, **active low** (driven low when on, pull-up when off).
- GIO inputs are pull-ups, **active low** (switch to GND sets the bit).
- The buzzer is push-pull, **active high**.
- DMX TX is a driven UART into a MAX485. It is not an active-low GPIO. DE and /RE tied high keep the driver on.

Board drawings and the MAX485 wiring stay in each repo: [MC pins](https://github.com/fablab-wue/DF_DMC_2_MC/blob/main/docs/pins.md), [PWM pins](https://github.com/fablab-wue/DF_DMC_2_PWM/blob/main/docs/pins.md), [MKS pins](https://github.com/fablab-wue/DF_DMC_2_MKS/blob/main/docs/pins.md).

### DF_DMC_2_MC (RP2040-Zero)

| Pad | Function | Level |
|-----|----------|-------|
| GP0 | DMX512 TX, PIO UART 250000 8N2, into a MAX485 | UART, driver always on |
| GP1 | GIO OUT0 | Open-collector, active low |
| GP2 | GIO OUT1 | Open-collector, active low |
| GP3 | GIO OUT2 | Open-collector, active low |
| GP4 | GIO OUT3 | Open-collector, active low |
| GP5 | GIO IN0 | Pull-up, active low |
| GP6 | GIO IN1 | Pull-up, active low |
| GP7 | GIO IN2 | Pull-up, active low |
| GP8 | GIO IN3 | Pull-up, active low |
| GP9 | Camera shutter, also SliderMC `CT` | Open-collector, active low |
| GP10 | Buzzer, also SliderMC `BE` | Push-pull, active high |
| GP11 | MOVE, high while SliderMC status is moving (`M`, `A`, `B`, `H`, `P`) | Push-pull, active high |
| GP12 | UART TX to SliderMC, 115200 | 3.3 V UART |
| GP13 | UART RX from SliderMC | 3.3 V UART |
| GP14 | DMX channel 6 PWM mirror, 18 kHz | High-active duty |
| GP15 | DMX channel 5 PWM mirror | High-active duty |
| GP16 | Onboard WS2812 status LED | Not a logic pin |
| GP26 | DMX channel 4 PWM mirror | High-active duty |
| GP27 | DMX channel 3 PWM mirror | High-active duty |
| GP28 | DMX channel 2 PWM mirror | High-active duty |
| GP29 | DMX channel 1 PWM mirror | High-active duty |
| USB | Dragonframe DMC | CDC, binary |

GP17–GP20 are not DMC GIO. SliderMC extender pins stay on the motion board.

### DF_DMC_2_PWM (Raspberry Pi Pico)

DIP switches choose how many of GP0–GP15 are servos. The rest of that range mirrors DMX, high-active, 18 kHz unless SW4 is on (about 2 kHz, squared). See the PWM repo `docs/dip.md`.

| Pad | Function | Level |
|-----|----------|-------|
| GP0–GP15 | Servo PWM and/or DMX PWM mirror | Servo pulse, or high-active DMX duty |
| GP16 | DMX512 TX into a MAX485 | UART, driver always on |
| GP17 | Camera shutter | Open-collector, active low |
| GP18 | DIP SW1 | Pull-up, ON = low |
| GP19 | DIP SW2 | Pull-up, ON = low |
| GP20 | DIP SW3 | Pull-up, ON = low |
| GP21 | DIP SW4 (DMX PWM curve) | Pull-up, ON = low |
| GP22 | Buzzer | Push-pull, active high |
| GP25 | Onboard LED | Not on the header |
| GP26 | GIO OUT0 | Open-collector, active low |
| GP27 | GIO OUT1 | Open-collector, active low |
| GP28 | GIO IN0 | Pull-up, active low |
| USB | Dragonframe DMC | CDC, binary |

GP23 (SMPS) and GP24 (VBUS detect) are left to the Pico.

### DF_DMC_2_MKS (RP2040-Zero)

| Pad | Function | Level |
|-----|----------|-------|
| GP0 | DMX512 TX into a MAX485. No DMX levels on the servo pins | UART, DE and /RE tied high |
| GP1 | Servo 1 (Dragonframe motor 9) | Hobby PWM, about 306 Hz |
| GP2 | Servo 2 (motor 10) | Hobby PWM |
| GP3 | Servo 3 (motor 11) | Hobby PWM |
| GP4 | Servo 4 (motor 12) | Hobby PWM |
| GP5 | Servo 5 (motor 13) | Hobby PWM |
| GP6 | Servo 6 (motor 14) | Hobby PWM |
| GP7 | Servo 7 (motor 15) | Hobby PWM |
| GP8 | Servo 8 (motor 16) | Hobby PWM |
| GP9 | Camera shutter | Open-collector, active low |
| GP10 | Buzzer | Push-pull, active high |
| GP11 | RS485 direction, DE and /RE | High = transmit, low = listen |
| GP12 | RS485 TX to the MKS bus, 256000 8N1 | 3.3 V into MAX485 DI |
| GP13 | RS485 RX. The MAX485 RO pin is 5 V; use the divider in the MKS pin page | 3.3 V after the divider |
| GP14 | GIO IN1 | Pull-up, active low |
| GP15 | GIO IN0 | Pull-up, active low |
| GP16 | Onboard WS2812 status LED | Not a logic pin |
| GP17–GP25 | Debug inputs, unused by the protocol | Inputs |
| GP26 | GIO OUT3 | Open-collector, active low |
| GP27 | GIO OUT2 | Open-collector, active low |
| GP28 | GIO OUT1 | Open-collector, active low |
| GP29 | GIO OUT0 | Open-collector, active low |
| USB | Dragonframe DMC | CDC, binary |

Two MAX485 chips, both on 5 V from the Zero’s 5V pad: one for DMX, one for the motor bus. They do not share A/B. Drawings are on the MKS pin page.
