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
