# Exile III: Ruined World for Modern Linux

A modernized, high-performance port wrapper and Docker container for Spiderweb Software's classic 1990s RPG **Exile III: Ruined World** (and the Exile III Character Editor) on 64-bit Linux.

---

## Features & Modern Enhancements

* **Non-Blocking Digital Sound Effects**: Custom `libexile3audio.so` interceptor shim intercepts legacy `/dev/dsp` and OSS audio calls, playing sound effects asynchronously via PulseAudio / PipeWire so the game engine no longer stutters or freezes during combat and spellcasting.
* **High-Performance Anti-Aliased Typography**: Fully modern FreeType 2 TrueType font renderer featuring:
  - Subpixel-accurate bytecode hinting and anti-aliasing.
  - RAM glyph caching and direct client-side XImage block blitting (< 1 millisecond full-screen text draw latency, eliminating legacy 500ms line-by-line scanning).
  - Proportional character metrics calibrated to match classic 1990s MS Sans Serif proportions without dialogue clipping.
* **Razor-Sharp Modern Scaling**: Integrated [Gamescope](https://github.com/ValveSoftware/gamescope) support for integer pixel-art scaling (1280x960, 1920x1440) and AMD FidelityFX Super Resolution (FSR).
* **Self-Contained Docker Container**: Run instantly on any modern 64-bit Linux distribution with a single `docker run` command—zero 32-bit multiarch or legacy library dependencies required on the host system.

---

## Quick Start (Docker)

To run without installing 32-bit libraries or compilation tools:

```bash
# Allow local X11 container display
xhost +local:docker

# Launch Exile III in fullscreen
docker run --rm -it --net=host --ipc=host \
    -e DISPLAY="$DISPLAY" \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$XDG_RUNTIME_DIR/pulse:$XDG_RUNTIME_DIR/pulse:ro" \
    -e PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native" \
    --device /dev/dri \
    -v "$(pwd)/saves:/game/saves" \
    ghcr.io/lavacano/exile3:latest
```

To launch the Character Editor instead:
```bash
docker run --rm -it --net=host --ipc=host \
    -e DISPLAY="$DISPLAY" \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v "$XDG_RUNTIME_DIR/pulse:$XDG_RUNTIME_DIR/pulse:ro" \
    -e PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native" \
    --device /dev/dri \
    -v "$(pwd)/saves:/game/saves" \
    ghcr.io/lavacano/exile3:latest editor
```

---

## Native Run (`run.sh`)

If you have cloned this repository and have Xephyr / Gamescope installed (or use Distrobox):

```bash
# Launch game in best-quality fullscreen (Gamescope + pixel-art filter)
./run.sh -f

# Fullscreen with AMD FSR edge smoothing
./run.sh -f --fsr

# Launch as a 2x scaled desktop window (1280x960)
./run.sh

# Launch the Exile III Character Editor
./run.sh -f editor
```

### Options Reference

| Flag | Description |
| :--- | :--- |
| `-f`, `--fullscreen` | Fullscreen with 4:3 aspect ratio preserved |
| `--stretch` | Fullscreen stretched to fill entire 16:9 / 16:10 monitor |
| `--pixel` | Razor-sharp pixel-art integer upscaling (default) |
| `--fsr` | AMD FidelityFX Super Resolution edge-adaptive filter |
| `--sharpness <0-20>` | FSR sharpness (0 = maximum sharp, 20 = soft, default: 1) |
| `--scale <1x\|2x\|3x>` | Window scale factor (default: 2x / 1280x960) |
| `--docker` | Run via the self-contained container image |
| `--no-aa` | Disable FreeType anti-aliasing (use legacy 1-bit bitmap fonts) |

---

## Building from Source

To compile the `libexile3audio.so` interceptor shim:
```bash
make
```

To build the Docker image locally:
```bash
docker build -t exile3:latest .
```

---

## License & Credits

* **Exile III: Ruined World** is copyright © 2000 Spiderweb Software and Boutell.com.
* FreeType typography engine, audio interceptor, and modern container wrapper developed by lavacano.
