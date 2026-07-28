#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

show_help() {
    cat <<'EOF'
Build, upload, and monitor SarcasmOS eye firmware.

Usage:
  ./flash.sh (--left | --right) [actions] [options]

Targets:
  --left                 Use the left-eye firmware (role 0, I2C address 0x30)
  --right                Use the right-eye firmware (role 1, I2C address 0x31)

Actions (run in this order regardless of argument order):
  --build                Configure and build the selected firmware
  --upload               Flash and verify it with J-Link/OpenOCD
  --monitor              Open its 115200-baud UART with minicom

Options:
  --port DEVICE          Serial device for --monitor (for example /dev/ttyACM0)
  --baud RATE            Monitor baud rate (default: 115200)
  -h, --help             Show this help

Environment overrides:
  OPENOCD_ROOT           OpenOCD installation containing bin/ and share/
  EYE_SERIAL_PORT        Serial device used for either eye
  EYE_LEFT_PORT          Serial device used for --left
  EYE_RIGHT_PORT         Serial device used for --right

Examples:
  ./flash.sh --build --upload --monitor --left
  ./flash.sh --right --build --upload
  ./flash.sh --left --monitor --port /dev/ttyACM0

For convenience, --monitor--left and --monitor--right are also accepted.
EOF
}

fail() {
    printf 'flash.sh: %s\n' "$*" >&2
    exit 1
}

eye=""
do_build=false
do_upload=false
do_monitor=false
serial_port=""
baud=115200

while (($#)); do
    case "$1" in
        --left)
            [[ -z "$eye" || "$eye" == left ]] || fail "choose only one of --left and --right"
            eye=left
            ;;
        --right)
            [[ -z "$eye" || "$eye" == right ]] || fail "choose only one of --left and --right"
            eye=right
            ;;
        --build) do_build=true ;;
        --upload) do_upload=true ;;
        --monitor) do_monitor=true ;;
        --monitor--left)
            [[ -z "$eye" || "$eye" == left ]] || fail "choose only one of --left and --right"
            eye=left
            do_monitor=true
            ;;
        --monitor--right)
            [[ -z "$eye" || "$eye" == right ]] || fail "choose only one of --left and --right"
            eye=right
            do_monitor=true
            ;;
        --port)
            (($# >= 2)) || fail "--port requires a device"
            serial_port=$2
            shift
            ;;
        --baud)
            (($# >= 2)) || fail "--baud requires a rate"
            baud=$2
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *) fail "unknown option: $1 (try --help)" ;;
    esac
    shift
done

[[ -n "$eye" ]] || fail "select an eye with --left or --right"
$do_build || $do_upload || $do_monitor || fail "select at least one action: --build, --upload, or --monitor"
[[ "$baud" =~ ^[0-9]+$ ]] && ((baud > 0)) || fail "invalid baud rate: $baud"

if [[ "$eye" == left ]]; then
    role=0
    i2c_address=0x30
    role_port=${EYE_LEFT_PORT:-}
else
    role=1
    i2c_address=0x31
    role_port=${EYE_RIGHT_PORT:-}
fi

build_dir="$script_dir/build-$eye"
image="$build_dir/sarcasmos_eye.elf"

if $do_build; then
    if [[ -z "${PICO_SDK_PATH:-}" ]]; then
        for cache_file in \
            "$build_dir/CMakeCache.txt" \
            "$script_dir/build-left/CMakeCache.txt" \
            "$script_dir/build-right/CMakeCache.txt"; do
            [[ -f "$cache_file" ]] || continue
            cached_sdk=$(sed -n 's/^PICO_SDK_PATH:PATH=//p' "$cache_file" | head -n 1)
            if [[ -f "$cached_sdk/external/pico_sdk_import.cmake" ]]; then
                export PICO_SDK_PATH=$cached_sdk
                printf 'Using cached Pico SDK: %s\n' "$PICO_SDK_PATH"
                break
            fi
        done
    fi
    [[ -f "${PICO_SDK_PATH:-}/external/pico_sdk_import.cmake" ]] || \
        fail "Pico SDK not found; export PICO_SDK_PATH before building"

    printf 'Configuring %s eye...\n' "$eye"
    cmake -S "$script_dir" -B "$build_dir" \
        -DDEVICE_ROLE="$role" \
        -DI2C_ADDRESS="$i2c_address"
    printf 'Building %s eye...\n' "$eye"
    cmake --build "$build_dir" --target sarcasmos_eye
fi

if $do_upload; then
    [[ -f "$image" ]] || fail "firmware not found at $image; run with --build first"

    openocd_bin=""
    openocd_scripts=""
    if [[ -n "${OPENOCD_ROOT:-}" ]]; then
        openocd_bin="$OPENOCD_ROOT/bin/openocd"
        openocd_scripts="$OPENOCD_ROOT/share/openocd/scripts"
        [[ -x "$openocd_bin" ]] || fail "OpenOCD is not executable at $openocd_bin"
        [[ -d "$openocd_scripts" ]] || fail "OpenOCD scripts not found at $openocd_scripts"
    elif command -v openocd >/dev/null 2>&1; then
        openocd_bin=$(command -v openocd)
    else
        shopt -s nullglob
        openocd_candidates=("${HOME}"/.espressif/tools/openocd-esp32/*/openocd-esp32)
        shopt -u nullglob
        ((${#openocd_candidates[@]})) || fail "OpenOCD not found; set OPENOCD_ROOT"
        openocd_root=${openocd_candidates[-1]}
        openocd_bin="$openocd_root/bin/openocd"
        openocd_scripts="$openocd_root/share/openocd/scripts"
        [[ -x "$openocd_bin" ]] || fail "OpenOCD is not executable at $openocd_bin"
        [[ -d "$openocd_scripts" ]] || fail "OpenOCD scripts not found at $openocd_scripts"
    fi

    printf 'Uploading %s eye with OpenOCD...\n' "$eye"
    openocd_args=()
    [[ -z "$openocd_scripts" ]] || openocd_args+=(-s "$openocd_scripts")
    "$openocd_bin" "${openocd_args[@]}" \
        -f interface/jlink.cfg \
        -c "adapter speed 100" \
        -c "set USE_CORE 0" \
        -f target/rp2040.cfg \
        -c "program $image verify reset exit"
fi

if $do_monitor; then
    command -v minicom >/dev/null 2>&1 || fail "minicom is required for --monitor"

    if [[ -z "$serial_port" ]]; then
        serial_port=${role_port:-${EYE_SERIAL_PORT:-}}
    fi
    if [[ -z "$serial_port" ]]; then
        shopt -s nullglob
        serial_candidates=(/dev/serial/by-id/*)
        if ((${#serial_candidates[@]} == 0)); then
            serial_candidates=(/dev/ttyACM* /dev/ttyUSB*)
        fi
        shopt -u nullglob

        if ((${#serial_candidates[@]} == 1)); then
            serial_port=${serial_candidates[0]}
        elif ((${#serial_candidates[@]} == 0)); then
            fail "no serial device found; connect the UART or pass --port DEVICE"
        else
            printf 'Multiple serial devices found:\n' >&2
            printf '  %s\n' "${serial_candidates[@]}" >&2
            fail "choose one with --port DEVICE or an EYE_LEFT_PORT/EYE_RIGHT_PORT override"
        fi
    fi
    [[ -e "$serial_port" ]] || fail "serial device does not exist: $serial_port"

    printf 'Monitoring %s eye on %s at %s baud (Ctrl-A, X to exit)...\n' \
        "$eye" "$serial_port" "$baud"
    exec minicom -D "$serial_port" -b "$baud"
fi
