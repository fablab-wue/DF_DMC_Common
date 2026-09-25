# Connecting a DF_DMC board to Dragonframe

[← Index](../README.md)

This is the same for [DF_DMC_2_MC](https://github.com/fablab-wue/DF_DMC_2_MC) and [DF_DMC_2_PWM](https://github.com/fablab-wue/DF_DMC_2_PWM).

## Connect

1. Close anything else that has the board’s COM port open (PlatformIO monitor, a terminal).
2. In Dragonframe: **Scene → Connections → Add connection**.
3. Device type **dmc-lite**.
4. Select this board’s serial port.
5. **Connect**.

The board sends a hello when the port opens, and answers Dragonframe’s `MSG_HI`. USB CDC is **binary DMC**. The serial monitor is not a text console.

Dragonframe’s own pages:

- [How do I integrate a motion control system?](https://www.dragonframe.com/ufaqs/how-do-i-integrate-a-motion-control-system-with-dragonframe/)
- [Where do I find the dmc-lite Arduino sketch?](https://www.dragonframe.com/ufaqs/where-do-i-find-the-dmc-lite-arduino-sketch/) — installed with Dragonframe, not in these repos
- [DMC v2 protocol PDF (2024-08-13)](https://www.dragonframe.com/download/dmcproto/DMC-Protocol-2024-08-13.pdf)

## Which scale

| Board | In Arc |
|-------|--------|
| DF_DMC_2_MC | **steps per unit = 1000** (1000 steps = 1 mm or 1 deg). [Overview](https://github.com/fablab-wue/DF_DMC_2_MC/blob/main/docs/overview.md) |
| DF_DMC_2_PWM | Use the step integers as they are. Do **not** set 1000. Pulse scale: [dragonframe.md](https://github.com/fablab-wue/DF_DMC_2_PWM/blob/main/docs/dragonframe.md) |

One DMC connection exposes at most 16 motors. The DIP on the PWM board chooses how many of those are servos.

## Go motion

Both `SHOOT_FRAME` (`0x0112`) and `SHOOT_FRAME2` (`0x0115`) are go motion. Dragonframe only offers them when the hello capabilities include the matching bit: `GO_MOTION` `0x0002` for `SHOOT_FRAME`, `GO_MOTION2` `0x0080` for `SHOOT_FRAME2`. Both boards advertise both bits.

Axes take part only when `MOTOR_CONFIGURE` has the blur flag `0x02`. Other axes hold the frame pose.

Sequence for both: the device prerolls and stops; `GO` before that is idle returns not-in-position (`0x0016`); `GO` runs the blur; the local camera shutter opens only while the axis should be at constant speed. A still does not send `MSG_RT_END` (that message is for live path playback).

### SHOOT_FRAME (`0x0112`)

Payload: frame dword, direction byte (`1` forward, `0` backward), exposure ms dword, blur percent × 10 (word; `0` means `1000` = 100%, `500` means 50%), then optional groups of motor byte, pos A dword, pos B dword.

The exposure segment is the interpolated path at frame ± (blur × 0.0005). Direction `0` runs that window backward. Accel and decel are each a fixed 1 second outside the shutter. The shutter opens when the 1 s ramp ends and closes when the exposure ends.

If Dragonframe sends pos A/B, those are the poses at shutter angle 0 and 360. The exposure uses the center of A→B shrunk by `(0.5 − |dt|) × (B − A)`.

### SHOOT_FRAME2 (`0x0115`)

Payload: frame dword, exposure ms dword (`0` means `1000`), shutter-open angle word, shutter-close angle word, then the same optional motor / pos A / pos B groups. No direction and no blur percent.

The motor move is one full frame of the path, from frame−0.5 to frame+0.5, or pos A to pos B when that motor was sent. That span includes the ramps.

Let degrees = close − open and Te = exposure in seconds. One degree lasts Te/degrees. The move lasts T = 360 × Te / degrees. Accel and decel are each 0.125×T; cruise is 0.75×T. Cruise speed is (8/7)×distance/T.

If the open angle is negative, the shutter opens at `GO` and the motors wait −open degrees before accelerating. If the close angle is past 360, the motors hold after the move. Otherwise the shutter opens open×secondsPerDegree after `GO` and closes one exposure later.

## Using go motion

1. Connect the DMC device (jDF-PWM or jDF-MC) as in [Connect](#connect).
2. In the arc / motion-control axis setup, enable the axis and turn on blur (go motion) for the axes that should move during the exposure. Axes without blur stay on the frame pose.
3. Upload or capture the move so the path is on the device.
4. In the go-motion / exposure controls, set exposure time and blur percent. That uses `SHOOT_FRAME`. A blur of 50% moves about half a frame of the path while the shutter is open, with one second of acceleration before and one second of deceleration after.
5. If Dragonframe offers shutter open/close angles (the second go-motion mode), that uses `SHOOT_FRAME2`: the axis travels one full frame, and the shutter opens and closes at those angles. Exposure time is the time between those angles.
6. Capture: the device moves to the pre-exposure pose and waits. When Dragonframe sends go, it ramps, opens the shutter for the exposure, then decelerates. Do not jog the axis until that frame finishes.

PWM servos cap blur speed at the axis max (default 40000 steps/s). The motion-controller board rejects a move that would exceed SliderMC max speed or max accel (`!E:speed`) and does not open the camera.
