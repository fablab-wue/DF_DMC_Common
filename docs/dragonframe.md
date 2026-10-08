# Go motion

[← User manual](manual.md)

Connect, steps per unit, features, limits, GIO, and pin tables are in the [user manual](manual.md). This page is the shoot-frame payload.

Dragonframe only offers these messages when the hello capabilities include the matching bit: `GO_MOTION` `0x0002` for `SHOOT_FRAME`, `GO_MOTION2` `0x0080` for `SHOOT_FRAME2`. MC, PWM, and MKS advertise both.

Using Dragonframe’s own description of an external rig: [Using Dragonframe 2025](https://www.dragonframe.com/download/Using%20Dragonframe%202025.pdf), Chapter 15, “Using Go-Motion with an External Motion Control System”, page 265.

## Messages

Axes take part only when `MOTOR_CONFIGURE` has the blur flag `0x02`. Other axes hold the frame pose. MC, PWM, and MKS advertise both shoot messages.

Sequence for both: the device prerolls and stops; `GO` before that is idle returns not-in-position (`0x0016`); `GO` runs the blur; the local camera shutter opens only while the axis should be at constant speed. When the exposure finishes, or the shoot is aborted, the device sends `MSG_RT_END`.

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

1. Connect the DMC device as in the [user manual](manual.md#connect). MC, PWM, and MKS run go motion.
2. In the arc / motion-control axis setup, enable the axis and turn on blur (go motion) for the axes that should move during the exposure. Axes without blur stay on the frame pose.
3. Upload or capture the move so the path is on the device.
4. In the go-motion / exposure controls, set exposure time and blur percent. That uses `SHOOT_FRAME`. A blur of 50% moves about half a frame of the path while the shutter is open, with one second of acceleration before and one second of deceleration after.
5. If Dragonframe offers shutter open/close angles (the second go-motion mode), that uses `SHOOT_FRAME2`: the axis travels one full frame, and the shutter opens and closes at those angles. Exposure time is the time between those angles.
6. Capture: the device moves to the pre-exposure pose and waits. When Dragonframe sends go, it ramps, opens the shutter for the exposure, then decelerates. Do not jog the axis until that frame finishes.

PWM servos cap blur speed at the axis max (default 40000 steps/s). The motion-controller board rejects a move that would exceed SliderMC max speed or max accel (`!E:speed`) and does not open the camera.
