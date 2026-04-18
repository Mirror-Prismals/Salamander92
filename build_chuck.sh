#!/usr/bin/env bash
set -euo pipefail

# Build libchuck.a without HID, OSC, or OTF server, skipping chuck_otf.cpp
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/third_party/chuck/build"
CORE_DIR="$ROOT_DIR/third_party/chuck/core"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

rm -f *.o libchuck.a
cp "$CORE_DIR/chuck_yacc.h" "$CORE_DIR/chuck.tab.h"

for f in "$CORE_DIR"/*.cpp; do
  base="$(basename "$f")"
  if [[ "$base" == "chuck_otf.cpp" ]]; then
    continue
  fi
  clang++ -std=c++17 -O2 -I"$CORE_DIR" \
    -D__PLATFORM_MACOSX__ -D__UNIX_JACK__ \
    -D__DISABLE_HID__ -D__DISABLE_OPSC__ -D__DISABLE_OTF_SERVER__ \
    -c "$f"
done

clang -std=c11 -O2 -I"$CORE_DIR" \
  -D__PLATFORM_MACOSX__ -D__UNIX_JACK__ \
  -D__DISABLE_HID__ -D__DISABLE_OPSC__ -D__DISABLE_OTF_SERVER__ \
  -c "$CORE_DIR/util_raw.c" "$CORE_DIR/util_xforms.c" "$CORE_DIR/util_sndfile.c" \
     "$CORE_DIR/util_network.c" "$CORE_DIR/chuck_yacc.c"

ar rcs libchuck.a *.o

echo "Built libchuck.a in $BUILD_DIR"
