#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

show_help() {
    cat <<'EOF'
Build, upload, and monitor SarcasmOS eye firmware.

Usage:
  ./flash.sh (--left | --right) [firmware] [actions] [options]

Targets:
  --left                 Use the left-eye firmware (role 0, I2C address 0x30)
  --right                Use the right-eye firmware (role 1, I2C address 0x31)

Firmware (default: --regular):
  --regular              Normal firmware controlled over I2C
  --self-test            Cycle through display test patterns
  --demo                 Cycle through every animation and log its name

Actions (run in this order regardless of argument order):
  --build                Configure and build the selected firmware
  --upload               Flash and verify it with J-Link/OpenOCD
  --monitor              Open its 115200-baud UART with minicom
  --swd-monitor          Stream state changes through SWD using RTT

Options:
  --port DEVICE          Serial device for --monitor (for example /dev/ttyACM0)
  --baud RATE            Monitor baud rate (default: 115200)
  --rtt-port PORT        Local RTT TCP port (default: 9090)
  -h, --help             Show this help

Environment overrides:
  OPENOCD_ROOT           OpenOCD installation containing bin/ and share/
  EYE_SERIAL_PORT        Serial device used for either eye
  EYE_LEFT_PORT          Serial device used for --left
  EYE_RIGHT_PORT         Serial device used for --right

Examples:
  ./flash.sh --left --regular --build --upload --swd-monitor
  ./flash.sh --left --self-test --build --upload --swd-monitor
  ./flash.sh --right --demo --build --upload --swd-monitor

For convenience, --monitor--left and --monitor--right are also accepted.
EOF
}

fail() {
    printf 'flash.sh: %s\n' "$*" >&2
    exit 1
}

eye=""
firmware=regular
firmware_selected=false
do_build=false
do_upload=false
do_monitor=false
do_swd_monitor=false
serial_port=""
baud=115200
rtt_port=9090

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
        --regular|--self-test|--demo)
            requested_firmware=${1#--}
            if $firmware_selected && [[ "$firmware" != "$requested_firmware" ]]; then
                fail "choose only one of --regular, --self-test, and --demo"
            fi
            firmware=$requested_firmware
            firmware_selected=true
            ;;
        --build) do_build=true ;;
        --upload) do_upload=true ;;
        --monitor) do_monitor=true ;;
        --swd-monitor) do_swd_monitor=true ;;
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
        --rtt-port)
            (($# >= 2)) || fail "--rtt-port requires a port number"
            rtt_port=$2
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
$do_build || $do_upload || $do_monitor || $do_swd_monitor || \
    fail "select at least one action: --build, --upload, --monitor, or --swd-monitor"
[[ "$baud" =~ ^[0-9]+$ ]] && ((baud > 0)) || fail "invalid baud rate: $baud"
[[ "$rtt_port" =~ ^[0-9]+$ ]] && ((rtt_port > 0 && rtt_port <= 65535)) || \
    fail "invalid RTT port: $rtt_port"
$do_monitor && $do_swd_monitor && fail "choose only one of --monitor and --swd-monitor"

if [[ "$eye" == left ]]; then
    role=0
    i2c_address=0x30
    role_port=${EYE_LEFT_PORT:-}
else
    role=1
    i2c_address=0x31
    role_port=${EYE_RIGHT_PORT:-}
fi

case "$firmware" in
    regular)
        build_dir="$script_dir/build-$eye"
        build_target=sarcasmos_eye
        autoplay=OFF
        ;;
    self-test)
        build_dir="$script_dir/build-self-test-$eye"
        build_target=sarcasmos_eye_display_test
        autoplay=OFF
        ;;
    demo)
        build_dir="$script_dir/build-demo-$eye"
        build_target=sarcasmos_eye
        autoplay=ON
        ;;
esac
image="$build_dir/$build_target.elf"

if $do_build; then
    if [[ -z "${PICO_SDK_PATH:-}" ]]; then
        for cache_file in \
            "$build_dir/CMakeCache.txt" \
            "$script_dir/build-left/CMakeCache.txt" \
            "$script_dir/build-right/CMakeCache.txt" \
            "$script_dir/build-test/CMakeCache.txt" \
            "$script_dir/build-animation-test/CMakeCache.txt"; do
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

    printf 'Configuring %s firmware for the %s eye...\n' "$firmware" "$eye"
    cmake -S "$script_dir" -B "$build_dir" \
        -DDEVICE_ROLE="$role" \
        -DI2C_ADDRESS="$i2c_address" \
        -DANIMATION_AUTOPLAY="$autoplay"
    printf 'Building %s firmware for the %s eye...\n' "$firmware" "$eye"
    cmake --build "$build_dir" --target "$build_target"
fi

if $do_upload || $do_swd_monitor; then
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
    openocd_args=()
    [[ -z "$openocd_scripts" ]] || openocd_args+=(-s "$openocd_scripts")
fi

if $do_upload; then
    printf 'Uploading %s firmware to the %s eye with OpenOCD...\n' "$firmware" "$eye"
    "$openocd_bin" "${openocd_args[@]}" \
        -f interface/jlink.cfg \
        -c "adapter speed 100" \
        -c "set USE_CORE 0" \
        -f target/rp2040.cfg \
        -c "program $image verify reset exit"
fi

if $do_swd_monitor; then
    command -v arm-none-eabi-nm >/dev/null 2>&1 || \
        fail "arm-none-eabi-nm is required for --swd-monitor"
    command -v nc >/dev/null 2>&1 || fail "nc is required for --swd-monitor"

    rtt_address=$(arm-none-eabi-nm "$image" | awk \
        '$3 == "swd_rtt_control_block" { print "0x" $1; exit }')
    [[ -n "$rtt_address" ]] || \
        fail "RTT control block not found in $image; rebuild the firmware"

    openocd_log=$(mktemp "${TMPDIR:-/tmp}/eye-openocd.XXXXXX")
    openocd_pid=""
    cleanup_swd_monitor() {
        if [[ -n "$openocd_pid" ]] && kill -0 "$openocd_pid" 2>/dev/null; then
            kill "$openocd_pid" 2>/dev/null || true
            wait "$openocd_pid" 2>/dev/null || true
        fi
        rm -f -- "$openocd_log"
    }
    trap cleanup_swd_monitor EXIT
    trap 'exit 130' INT TERM

    if nc -z 127.0.0.1 "$rtt_port" 2>/dev/null; then
        fail "RTT port $rtt_port is already in use; choose another with --rtt-port"
    fi

    printf 'Starting non-halting SWD monitor for %s firmware on the %s eye...\n' \
        "$firmware" "$eye"
    "$openocd_bin" "${openocd_args[@]}" \
        -c "gdb port disabled" \
        -c "tcl port disabled" \
        -c "telnet port disabled" \
        -f interface/jlink.cfg \
        -c "adapter speed 100" \
        -c "set USE_CORE 0" \
        -f target/rp2040.cfg \
        -c "init" \
        -c "resume" \
        -c "rtt setup $rtt_address 0x100" \
        -c "rtt polling_interval 20" \
        -c "rtt start" \
        -c "rtt server start $rtt_port 0" \
        >"$openocd_log" 2>&1 &
    openocd_pid=$!

    printf 'Streaming RTT state messages (Ctrl-C to exit)...\n'
    while ! nc 127.0.0.1 "$rtt_port" 2>/dev/null; do
        if ! kill -0 "$openocd_pid" 2>/dev/null; then
            printf 'OpenOCD failed to start:\n' >&2
            sed -n '1,160p' "$openocd_log" >&2
            fail "could not start the SWD monitor"
        fi
        sleep 0.1
    done
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

    printf 'Monitoring %s firmware on the %s eye at %s, %s baud (Ctrl-A, X to exit)...\n' \
        "$firmware" "$eye" "$serial_port" "$baud"
    exec minicom -D "$serial_port" -b "$baud"
fi
