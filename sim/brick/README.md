# OpenTracker on the TrimUI Brick

The desktop simulator built for aarch64 Linux, started in **handheld mode**. It
shows only the 800x480 screen, scaled to fill the display, and is played with
the Brick's buttons. `src/core` is the same as on the Tracker.

Status: **builds, but has not been tried on a device yet.** Device bring-up
notes are at the end of this file.

## Install

1. Build (below) or download the `brick` CI artifact.
2. Copy `Apps/OpenTracker/` to the `Apps/` folder of the Brick's SD card
   (stock TrimUI OS).
3. Start **OpenTracker** from the Apps list.

Everything stays in `Apps/OpenTracker/`:

| File | What it holds |
|---|---|
| `sd-card/` | the Tracker's SD card (projects, samples) |
| `sd-card.keys.txt` | the control map, written with the defaults on first start |
| `sd-card.eeprom` | settings |
| `sd-card.log` | the log of the last run |

## Controls

Hold **SELECT**, **L2** or **R2** to see that layer's controls on screen.

| Button | Alone | SELECT held (screens) | L2 held (editing) | R2 held (screen keys) |
|---|---|---|---|---|
| D-pad | arrows | Up Pattern, Down Song, Left Instr. Params, Right Sample Loader | Up Copy, Left/Right/Down arrows | Left F1, Up F2, Right F3, Down F4 |
| A | encoder push | Master | Note | F7 |
| B | Delete | File | Instr | F8 |
| X | Insert | Config | FX1 | F6 |
| Y | Shift | Performance | FX2 | F5 |
| L1 / R1 | encoder - / + (held: repeat) | Sample Editor / Sample Playback | Aux / Help | encoder - / + |
| START | Play | Sample Recorder | Rec | Power |
| MENU | pad grid | **quit** | | |
| L2 / R2 | (layer) | Aux / Help | | |

**Pad grid** (MENU): the 12x4 pads over the bottom of the screen, lit like the
Tracker's pads. Move with the D-pad, play with A (held = note held), close with
B or MENU. With a layer held, the buttons keep their layer meaning.

Edit `sd-card.keys.txt` to change any of this. A line is
`[LAYER+]BUTTON = TARGET`, and the file's header lists the names.

## Build

With Docker (Linux, macOS, WSL), from the repository root:

```
sh sim/brick/build.sh            # -> dist/OpenTracker-Brick/Apps/OpenTracker
```

Without the script: `docker build -t opentracker-brick sim/brick`, then
`make -C sim PLATFORM=brick` inside it. The toolchain is Debian 11, so glibc
2.31 or newer is needed on the device. libstdc++ is linked in, and SDL2 comes
from the device. USB MIDI (ALSA) is off unless you build with `BRICK_ALSA=1`,
because not every handheld OS ships libasound.

Test the handheld mode on a desktop with any gamepad (or the keyboard: arrows,
X=A, Z=B, S=X, A=Y, Q/W=L1/R1, 1/2=L2/R2, Enter=Start, Backspace=Select,
Esc=Menu):

```
build/sim/opentracker-sim --handheld
```

`make -C sim handheld-check` checks the control logic without a window.

## Device bring-up (not yet verified)

- **Buttons**: if SDL knows the Brick as a game controller, the log shows
  `controller "<name>"`. Otherwise it shows `joystick "<name>"`, and each press
  logs `joystick button N (unmapped)`. Map those with `JOY N = A` lines in
  `sd-card.keys.txt`. Alternatively, put an SDL mapping line in
  `gamecontrollerdb.txt` next to `launch.sh`. If A/B or X/Y come out swapped,
  swap them in the map.
- **Screen**: the log prints `display WxH`. The image keeps its 5:3 shape with
  black bars. If nothing shows, try `SDL_VIDEODRIVER=mali` or `kmsdrm` in
  `launch.sh`.
- **Audio**: through SDL's audio, at 44.1 kHz. If you hear underruns, the CPU
  meter (Config > General) shows the load.
- **SDL2 missing**: put `libSDL2-2.0.so.0` built for the device into
  `Apps/OpenTracker/lib/`.
- **Other launchers** (NextUI, Knulli, ...): only the packaging differs. The
  binary and `launch.sh` stay the same.
