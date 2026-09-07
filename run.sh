#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Defaults
SCALE="2x"
FULLSCREEN=false
STRETCH=false
TARGET="exile3"
FILTER="pixel"
SHARPNESS=1
USE_DOCKER=false
COLOR_DEPTH=24
AA_MODE="grayscale"
DOCKER_IMAGE="${EXILE_DOCKER_IMAGE:-ghcr.io/lavacano/exile3:latest}"

print_help() {
    cat << 'EOF'
Usage: ./run.sh [OPTIONS] [editor]

Options:
  -f, --fullscreen   Run fullscreen (4:3 aspect ratio preserved)
  --stretch          Run fullscreen stretched to fill entire screen (no black bars)
  --pixel            Use razor-sharp pixel-art reconstruction filter (default in fullscreen)
  --fsr              Use AMD FidelityFX Super Resolution edge-adaptive filter
  --sharpness <0-20> Set FSR sharpness level: 0 (max sharpness) to 20 (soft) (default: 1)
  --scale <factor>   Window scale factor: 1x, 2x (default: 1280x960), 3x (1920x1440)
  --native           Run native 640x480 without scaling (direct Xephyr, no gamescope)
  --aa               Use FreeType TrueType anti-aliasing + bytecode hinting (default)
  --no-aa            Disable FreeType anti-aliasing (legacy 1-bit bitmap fonts)
  --8bit             Force legacy 8-bit PseudoColor mode instead of 24-bit TrueColor
  --docker           Run using the self-contained exile3:latest Docker container
  editor, exile3ed   Launch the Exile III character editor instead of the game
  -h, --help         Show this help message

Examples:
  ./run.sh                     # Launch game with anti-aliased TrueType fonts (1280x960 window)
  ./run.sh -f                  # Best quality fullscreen with anti-aliased typography
  ./run.sh -f --fsr            # Fullscreen with AMD FSR edge-smoothing filter
  ./run.sh -f --stretch        # Fullscreen stretched to fill entire 16:10 / 16:9 monitor
  ./run.sh -f --docker         # Fullscreen running inside the portable Docker container
  ./run.sh -f editor           # Character editor in best quality fullscreen
EOF
    exit 0
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            print_help
            ;;
        -f|--fullscreen)
            FULLSCREEN=true
            shift
            ;;
        --stretch)
            STRETCH=true
            FULLSCREEN=true
            shift
            ;;
        --pixel)
            FILTER="pixel"
            shift
            ;;
        --fsr)
            FILTER="fsr"
            shift
            ;;
        --sharpness)
            SHARPNESS="$2"
            shift 2
            ;;
        --docker)
            USE_DOCKER=true
            shift
            ;;
        --scale)
            SCALE="$2"
            shift 2
            ;;
        --scale=*)
            SCALE="${1#*=}"
            shift
            ;;
        --native|1x)
            SCALE="1x"
            shift
            ;;
        2x|3x)
            SCALE="$1"
            shift
            ;;
        --aa)
            AA_MODE="grayscale"
            shift
            ;;
        --no-aa)
            AA_MODE="none"
            shift
            ;;
        --8bit)
            COLOR_DEPTH=8
            shift
            ;;
        editor|exile3ed)
            TARGET="editor"
            shift
            ;;
        *)
            TARGET="$1"
            shift
            ;;
    esac
done

if [ "$TARGET" = "editor" ] || [ "$TARGET" = "exile3ed" ]; then
    BIN="./exile3ed-binary"
    TITLE="Exile III Character Editor"
else
    BIN="./exile3-binary"
    TITLE="Exile III: Ruined World"
fi

# Check Gamescope availability
USE_GAMESCOPE=false
if command -v gamescope >/dev/null 2>&1; then
    if [ "$FULLSCREEN" = true ] || [ "$SCALE" != "1x" ]; then
        USE_GAMESCOPE=true
    fi
fi

if [ "$USE_GAMESCOPE" = true ]; then
    SCALER_MODE="fit"
    if [ "$STRETCH" = true ]; then
        SCALER_MODE="stretch"
    fi

    if [ "$FULLSCREEN" = true ]; then
        if [ "$FILTER" = "fsr" ]; then
            GS_ARGS=(-w 640 -h 480 -r 60 -b -f -S "$SCALER_MODE" -F fsr --fsr-sharpness "$SHARPNESS")
        else
            # Default pixel filter: razor-sharp, zero-blur retro pixel art scaling
            GS_ARGS=(-w 640 -h 480 -r 60 -b -f -S "$SCALER_MODE" -F pixel)
        fi
    elif [ "$SCALE" = "3x" ]; then
        GS_ARGS=(-w 640 -h 480 -r 60 -W 1920 -H 1440 -S integer -F "$FILTER")
    else
        # Default 2x (1280x960 clean integer scale for 640x480)
        GS_ARGS=(-w 640 -h 480 -r 60 -W 1280 -H 960 -S integer -F "$FILTER")
    fi

    # Clean up previous session sockets quickly
    if pgrep -f "Xephyr :2" >/dev/null 2>&1; then
        pkill -9 -f "Xephyr :2" 2>/dev/null || true
    fi
    rm -f /tmp/.X11-unix/X2 2>/dev/null || true

    NO_AA_VAL="0"
    if [ "$AA_MODE" = "none" ]; then
        NO_AA_VAL="1"
    fi
    EXILE_8BIT_VAL="0"
    if [ "$COLOR_DEPTH" = "8" ]; then
        EXILE_8BIT_VAL="1"
    fi

    if [ "$USE_DOCKER" = true ]; then
        if docker image inspect exile3:latest >/dev/null 2>&1; then
            DOCKER_IMAGE="exile3:latest"
        fi
        exec gamescope "${GS_ARGS[@]}" -- bash -c '
            xhost + >/dev/null 2>&1 || true
            mkdir -p "'"$DIR"'/saves"
            docker run --rm -i --net=host --ipc=host \
                -e DISPLAY="$DISPLAY" \
                -e EXILE_AA="'"$AA_MODE"'" \
                -e EXILE_NO_AA="'"$NO_AA_VAL"'" \
                -e EXILE_8BIT="'"$EXILE_8BIT_VAL"'" \
                -e TWINRC="/game/twinrc" \
                -v /tmp/.X11-unix:/tmp/.X11-unix \
                -v "$XDG_RUNTIME_DIR/pulse:$XDG_RUNTIME_DIR/pulse:ro" \
                -e PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native" \
                --device /dev/dri \
                -v "'"$DIR"'/saves:/game/saves" \
                "'"$DOCKER_IMAGE"'" "'"$TARGET"'"
        '
    else
        exec gamescope "${GS_ARGS[@]}" -- bash -c '
            distrobox enter exile3 -- Xephyr :2 -screen "640x480x'"$COLOR_DEPTH"'" +bs -nolisten tcp -title "'"$TITLE"'" -ac -fp "'"$DIR"'/fonts" -noxv -nodri -s 0 -dpms -noreset &
            XEP_PID=$!
            cleanup() {
                kill -9 $XEP_PID 2>/dev/null || true
                pkill -9 -f "Xephyr :2" 2>/dev/null || true
                rm -f /tmp/.X11-unix/X2 2>/dev/null || true
            }
            trap cleanup EXIT INT TERM
            for i in $(seq 1 100); do
                if [ -S /tmp/.X11-unix/X2 ]; then
                    break
                fi
                sleep 0.01
            done
            distrobox enter exile3 -- sh -c "cd '"$DIR"' && export DISPLAY=:2 && export EXILE_PATH='"$DIR"' && export LD_LIBRARY_PATH='"$DIR"' && export LD_PRELOAD='"$DIR"'/libexile3audio.so && export TWINRC='"$DIR"'/twinrc && export EXILE_AA='"$AA_MODE"' && export EXILE_NO_AA='"$NO_AA_VAL"' && export PULSE_LATENCY_MSEC=30 && export PADSP_NO_MIXER=1 && export PADSP_NO_SNDSTAT=1 && padsp '"$BIN"'"
            cleanup
        '
    fi
else
    # Direct Xephyr (Native 640x480 on desktop without Gamescope)
    if [ "$FULLSCREEN" = true ]; then
        echo "Note: 'gamescope' is not installed; running in native 640x480 window."
        echo "To enable crisp fullscreen scaling (with integer scaling or AMD FSR), install gamescope:"
        echo "  Ubuntu/Debian: sudo apt install gamescope"
        echo "  Arch Linux:    sudo pacman -S gamescope"
        echo "  Fedora:        sudo dnf install gamescope"
        echo ""
    fi
    NO_AA_VAL="0"
    if [ "$AA_MODE" = "none" ]; then
        NO_AA_VAL="1"
    fi
    EXILE_8BIT_VAL="0"
    if [ "$COLOR_DEPTH" = "8" ]; then
        EXILE_8BIT_VAL="1"
    fi

    if [ "$USE_DOCKER" = true ]; then
        if docker image inspect exile3:latest >/dev/null 2>&1; then
            DOCKER_IMAGE="exile3:latest"
        fi
        xhost + >/dev/null 2>&1 || true
        mkdir -p "$DIR/saves"
        exec docker run --rm -i --net=host --ipc=host \
            -e DISPLAY="$DISPLAY" \
            -e EXILE_AA="$AA_MODE" \
            -e EXILE_NO_AA="$NO_AA_VAL" \
            -e EXILE_8BIT="$EXILE_8BIT_VAL" \
            -e TWINRC="/game/twinrc" \
            -v /tmp/.X11-unix:/tmp/.X11-unix \
            -v "$XDG_RUNTIME_DIR/pulse:$XDG_RUNTIME_DIR/pulse:ro" \
            -e PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native" \
            --device /dev/dri \
            -v "$DIR/saves:/game/saves" \
            "$DOCKER_IMAGE" "$TARGET"
    else
        if pgrep -f "Xephyr :1" >/dev/null 2>&1; then
            pkill -9 -f "Xephyr :1" 2>/dev/null || true
        fi
        rm -f /tmp/.X11-unix/X1 2>/dev/null || true
        sleep 0.05

        distrobox enter exile3 -- Xephyr :1 -screen "640x480x$COLOR_DEPTH" +bs -nolisten tcp -title "$TITLE" -ac -fp "$DIR/fonts" -noxv -nodri -s 0 -dpms -noreset &
        XEPHYR_PID=$!

        cleanup() {
            kill -9 "$XEPHYR_PID" 2>/dev/null || true
            pkill -9 -f "Xephyr :1" 2>/dev/null || true
            rm -f /tmp/.X11-unix/X1 2>/dev/null || true
        }
        trap cleanup EXIT INT TERM

        for i in $(seq 1 100); do
            if [ -S /tmp/.X11-unix/X1 ]; then
                break
            fi
            sleep 0.01
        done

        distrobox enter exile3 -- sh -c "cd '$DIR' && export DISPLAY=:1 && export EXILE_PATH='$DIR' && export LD_LIBRARY_PATH='$DIR' && export LD_PRELOAD='$DIR/libexile3audio.so' && export TWINRC='$DIR/twinrc' && export EXILE_AA='$AA_MODE' && export EXILE_NO_AA='$NO_AA_VAL' && export PULSE_LATENCY_MSEC=30 && export PADSP_NO_MIXER=1 && export PADSP_NO_SNDSTAT=1 && padsp '$BIN'"
        cleanup
    fi

fi
