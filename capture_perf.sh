#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_BIN="${ROOT_DIR}/cardinal"

seconds="30"
logs="perf,frame,step,terrain,voxel,hitch"
output=""
tee_console="0"
dry_run="0"
headless="0"

usage() {
  cat <<'EOF'
Usage: ./capture_perf.sh [options] [-- <extra app args>]

Options:
  -s, --seconds <N>     Capture duration in seconds (default: 30)
  -l, --logs <csv>      Categories (default: perf,frame,step,terrain,voxel,hitch)
  -o, --output <path>   Output log file path (default: /tmp/salamander_perf_capture_<ts>.log)
      --tee             Keep captured categories in console output too
      --headless        Run perf capture without renderer/window init
      --dry-run         Print final command and exit without launching
  -h, --help            Show this help

Examples:
  ./capture_perf.sh --seconds 45 --logs terrain,hitch,frame
  ./capture_perf.sh -s 60 -o /tmp/mirror_run.log --tee
EOF
}

extra_args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -s|--seconds)
      seconds="${2:-}"
      shift 2
      ;;
    --seconds=*)
      seconds="${1#*=}"
      shift
      ;;
    -l|--logs)
      logs="${2:-}"
      shift 2
      ;;
    --logs=*)
      logs="${1#*=}"
      shift
      ;;
    -o|--output)
      output="${2:-}"
      shift 2
      ;;
    --output=*)
      output="${1#*=}"
      shift
      ;;
    --tee)
      tee_console="1"
      shift
      ;;
    --headless)
      headless="1"
      shift
      ;;
    --dry-run)
      dry_run="1"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      while [[ $# -gt 0 ]]; do
        extra_args+=("$1")
        shift
      done
      ;;
    *)
      extra_args+=("$1")
      shift
      ;;
  esac
done

if ! [[ "$seconds" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
  echo "Invalid --seconds value: ${seconds}" >&2
  exit 1
fi

if ! awk "BEGIN { exit (($seconds > 0) ? 0 : 1) }"; then
  echo "Invalid --seconds value: ${seconds}" >&2
  exit 1
fi

if [[ -z "$output" ]]; then
  output="/tmp/salamander_perf_capture_$(date +%s).log"
fi

cmd=(
  "$APP_BIN"
  --perf-capture
  --perf-seconds "$seconds"
  --perf-logs "$logs"
  --perf-log-file "$output"
)

if [[ "$tee_console" == "1" ]]; then
  cmd+=(--perf-tee-console)
fi

if [[ "$headless" == "1" ]]; then
  cmd+=(--perf-headless)
fi

if [[ ${#extra_args[@]} -gt 0 ]]; then
  cmd+=("${extra_args[@]}")
fi

echo "Perf capture output: ${output}"
echo "Running: ${cmd[*]}"

if [[ "$dry_run" == "1" ]]; then
  exit 0
fi

if [[ ! -x "$APP_BIN" ]]; then
  echo "Missing ${APP_BIN}. Build first with ./build_main.sh" >&2
  exit 1
fi

"${cmd[@]}"
