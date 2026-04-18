#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

APP_NAME="${CARDINAL_APP_NAME:-Cardinal}"
BUNDLE_ID="${CARDINAL_BUNDLE_ID:-com.salamander.cardinal}"
SHORT_VERSION="${CARDINAL_SHORT_VERSION:-0.1.0}"
BUNDLE_VERSION="${CARDINAL_BUNDLE_VERSION:-0.1.0}"
OUTPUT_DIR="$ROOT_DIR/dist"
BUILD_FIRST=0
CODESIGN_IDENTITY="${CARDINAL_CODESIGN_IDENTITY:--}"

usage() {
  cat <<'EOF'
Usage: ./package_macos_app.sh [options]

Options:
  --build                    Rebuild the project before packaging.
  --output-dir <dir>         Output directory for the .app bundle.
  --app-name <name>          CFBundleName / bundle folder name.
  --bundle-id <id>           CFBundleIdentifier.
  --short-version <ver>      CFBundleShortVersionString.
  --bundle-version <ver>     CFBundleVersion.
  --codesign <identity>      Codesign nested binaries and the app bundle.
                             Use '-' for ad-hoc signing. Default is ad-hoc.
  --no-codesign              Skip codesigning entirely.
  --help                     Show this help.

Environment overrides:
  CARDINAL_APP_NAME
  CARDINAL_BUNDLE_ID
  CARDINAL_SHORT_VERSION
  CARDINAL_BUNDLE_VERSION
  CARDINAL_CODESIGN_IDENTITY
  CARDINAL_JACKD
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build)
      BUILD_FIRST=1
      shift
      ;;
    --output-dir)
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --app-name)
      APP_NAME="$2"
      shift 2
      ;;
    --bundle-id)
      BUNDLE_ID="$2"
      shift 2
      ;;
    --short-version)
      SHORT_VERSION="$2"
      shift 2
      ;;
    --bundle-version)
      BUNDLE_VERSION="$2"
      shift 2
      ;;
    --codesign)
      CODESIGN_IDENTITY="$2"
      shift 2
      ;;
    --no-codesign)
      CODESIGN_IDENTITY=""
      shift
      ;;
    --help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ "$BUILD_FIRST" == "1" ]]; then
  "$ROOT_DIR/build_main.sh"
fi

MAIN_BINARY_SRC="$ROOT_DIR/cardinal"
if [[ ! -x "$MAIN_BINARY_SRC" ]]; then
  echo "Missing built executable: $MAIN_BINARY_SRC" >&2
  echo "Run ./build_main.sh first or pass --build." >&2
  exit 1
fi

JACKD_SRC="${CARDINAL_JACKD:-}"
if [[ -z "$JACKD_SRC" ]]; then
  JACKD_SRC="$(command -v jackd || true)"
fi
if [[ -z "$JACKD_SRC" || ! -x "$JACKD_SRC" ]]; then
  echo "Could not locate jackd. Install JACK or set CARDINAL_JACKD." >&2
  exit 1
fi

APP_DIR="$OUTPUT_DIR/$APP_NAME.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"
HELPERS_DIR="$CONTENTS_DIR/Helpers"
RESOURCES_DIR="$CONTENTS_DIR/Resources"
DATA_DIR="$RESOURCES_DIR/CardinalData"
MAIN_BINARY_DST="$MACOS_DIR/cardinal-bin"
LAUNCHER_DST="$MACOS_DIR/$APP_NAME"
JACKD_DST="$HELPERS_DIR/jackd"
INFO_PLIST="$CONTENTS_DIR/Info.plist"

if [[ -e "$APP_DIR" ]]; then
  BACKUP_PATH="$OUTPUT_DIR/$APP_NAME.previous.$(date +%s).app"
  mv "$APP_DIR" "$BACKUP_PATH"
  echo "Moved existing bundle to $BACKUP_PATH"
fi

mkdir -p "$MACOS_DIR" "$FRAMEWORKS_DIR" "$HELPERS_DIR" "$RESOURCES_DIR"

copy_data_dir() {
  local src="$1"
  local dst_parent="$2"
  cp -R "$src" "$dst_parent/"
}

echo "Copying runtime data..."
mkdir -p "$DATA_DIR"
copy_data_dir "$ROOT_DIR/Entities" "$DATA_DIR"
copy_data_dir "$ROOT_DIR/Levels" "$DATA_DIR"
copy_data_dir "$ROOT_DIR/Mirrors" "$DATA_DIR"
copy_data_dir "$ROOT_DIR/Procedures" "$DATA_DIR"
copy_data_dir "$ROOT_DIR/Systems" "$DATA_DIR"
mkdir -p "$DATA_DIR/BaseSystem" "$DATA_DIR/Host"
cp "$ROOT_DIR/BaseSystem/registry.json" "$DATA_DIR/BaseSystem/"
cp "$ROOT_DIR/BaseSystem/circuit_breaker.json" "$DATA_DIR/BaseSystem/"
cp "$ROOT_DIR/Host/perf.json" "$DATA_DIR/Host/"
if [[ -d "$DATA_DIR/Procedures/soundtrack" ]]; then
  find "$DATA_DIR/Procedures/soundtrack" -maxdepth 1 -type f -name '.salamander_soundtrack_wrapped_*.ck' -delete
fi

echo "Copying executables..."
cp "$MAIN_BINARY_SRC" "$MAIN_BINARY_DST"
cp -L "$JACKD_SRC" "$JACKD_DST"
chmod 755 "$MAIN_BINARY_DST" "$JACKD_DST"

cat > "$LAUNCHER_DST" <<EOF
#!/bin/bash
set -euo pipefail
SCRIPT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
CONTENTS_DIR="\$(cd "\$SCRIPT_DIR/.." && pwd)"
DATA_DIR="\$CONTENTS_DIR/Resources/CardinalData"
LOG_DIR="\${HOME}/Library/Logs/Cardinal"
LOG_FILE="\$LOG_DIR/launcher.log"

mkdir -p "\$LOG_DIR" || true
exec >>"\$LOG_FILE" 2>&1

echo "=== \$(date '+%Y-%m-%d %H:%M:%S') ==="
echo "Launching Cardinal.app"
echo "SCRIPT_DIR=\$SCRIPT_DIR"
echo "CONTENTS_DIR=\$CONTENTS_DIR"
echo "DATA_DIR=\$DATA_DIR"

if [[ ! -d "\$DATA_DIR" ]]; then
  echo "Missing data directory: \$DATA_DIR"
  exit 1
fi

cd "\$DATA_DIR"
echo "PWD=\$(pwd)"

"\$SCRIPT_DIR/cardinal-bin" "\$@"
status=\$?
echo "cardinal-bin exited with status \$status"
exit "\$status"
EOF
chmod 755 "$LAUNCHER_DST"

cat > "$INFO_PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>$APP_NAME</string>
  <key>CFBundleIdentifier</key>
  <string>$BUNDLE_ID</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>$APP_NAME</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>$SHORT_VERSION</string>
  <key>CFBundleVersion</key>
  <string>$BUNDLE_VERSION</string>
  <key>LSMinimumSystemVersion</key>
  <string>12.0</string>
  <key>NSHighResolutionCapable</key>
  <true/>
</dict>
</plist>
EOF

is_system_dep() {
  local dep="$1"
  case "$dep" in
    /System/*|/usr/lib/*)
      return 0
      ;;
    @*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

queue=()
queued_targets=()

has_array_value() {
  local needle="$1"
  shift || true
  local value
  for value in "$@"; do
    if [[ "$value" == "$needle" ]]; then
      return 0
    fi
  done
  return 1
}

enqueue_target() {
  local target="$1"
  if [[ ${#queued_targets[@]} -gt 0 ]] && has_array_value "$target" "${queued_targets[@]}"; then
    return
  fi
  queued_targets+=("$target")
  queue+=("$target")
}

bundled_names=()

bundle_dependency() {
  local dep="$1"
  local dep_name
  dep_name="$(basename "$dep")"
  local dst="$FRAMEWORKS_DIR/$dep_name"

  if [[ ${#bundled_names[@]} -gt 0 ]] && has_array_value "$dep_name" "${bundled_names[@]}"; then
    echo "$dst"
    return
  fi

  if [[ ! -e "$dep" ]]; then
    echo "Missing dependency: $dep" >&2
    exit 1
  fi

  cp -L "$dep" "$dst"
  chmod 755 "$dst"
  bundled_names+=("$dep_name")
  enqueue_target "$dst"
  echo "$dst"
}

patch_target_deps() {
  local target="$1"
  local install_prefix="$2"
  local target_name
  target_name="$(basename "$target")"

  while IFS= read -r dep; do
    [[ -z "$dep" ]] && continue
    if is_system_dep "$dep"; then
      continue
    fi

    local dep_name
    dep_name="$(basename "$dep")"
    if [[ "$dep_name" == "$target_name" ]]; then
      continue
    fi

    bundle_dependency "$dep" >/dev/null
    install_name_tool -change "$dep" "$install_prefix/$dep_name" "$target"
  done < <(otool -L "$target" | tail -n +2 | awk '{print $1}')
}

echo "Bundling non-system dylibs..."
enqueue_target "$MAIN_BINARY_DST"
enqueue_target "$JACKD_DST"

processed_targets=()
while [[ ${#queue[@]} -gt 0 ]]; do
  current="${queue[0]}"
  queue=("${queue[@]:1}")

  if [[ "$current" == "$FRAMEWORKS_DIR/"* ]]; then
    install_name_tool -id "@loader_path/$(basename "$current")" "$current"
    patch_target_deps "$current" "@loader_path"
  else
    patch_target_deps "$current" "@executable_path/../Frameworks"
  fi
  processed_targets+=("$current")
done

if [[ -n "$CODESIGN_IDENTITY" ]]; then
  echo "Codesigning nested dylibs and executables..."
  CODESIGN_ARGS=(--force --sign "$CODESIGN_IDENTITY")
  if [[ "$CODESIGN_IDENTITY" != "-" ]]; then
    CODESIGN_ARGS+=(--timestamp)
  fi
  while IFS= read -r dylib; do
    codesign "${CODESIGN_ARGS[@]}" "$dylib"
  done < <(find "$FRAMEWORKS_DIR" -type f -name '*.dylib' | sort)
  codesign "${CODESIGN_ARGS[@]}" "$JACKD_DST"
  codesign "${CODESIGN_ARGS[@]}" "$MAIN_BINARY_DST"
  codesign "${CODESIGN_ARGS[@]}" "$LAUNCHER_DST"
  codesign "${CODESIGN_ARGS[@]}" "$APP_DIR"
fi

echo
echo "Packaged app bundle:"
echo "  $APP_DIR"
echo
echo "Bundled helper runtime:"
echo "  $JACKD_DST"
echo
echo "Launch target:"
echo "  $LAUNCHER_DST"
echo
echo "Note: packaged Cardinal now autodetects the bundled jackd helper at runtime."
echo "If no JACK server is already running, AudioSystem will try to launch the bundled helper."
if [[ -n "$CODESIGN_IDENTITY" ]]; then
  if [[ "$CODESIGN_IDENTITY" == "-" ]]; then
    echo "The bundle was ad-hoc signed for local macOS launch."
  else
    echo "The bundle was signed with identity: $CODESIGN_IDENTITY"
  fi
else
  echo "Warning: bundle was left unsigned."
fi
