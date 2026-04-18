#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

EXTRA_CXXFLAGS=()
EXTRA_LIBS=()
EXTRA_SOURCES=(
  "$ROOT_DIR/main.cpp"
  "$ROOT_DIR/Host/Vst3UI.mm"
)

if [[ "$(uname -s)" == "Darwin" ]]; then
  EXTRA_SOURCES+=("$ROOT_DIR/Platform/WebGPUCocoaBridge.mm")
  EXTRA_LIBS+=("-framework" "QuartzCore")
fi

if [[ "${SALAMANDER_ENABLE_WEBGPU:-1}" == "1" ]]; then
  EXTRA_CXXFLAGS+=("-DSALAMANDER_ENABLE_WEBGPU=1")
fi

if [[ "${SALAMANDER_LINK_WEBGPU:-1}" == "1" ]]; then
  EXTRA_CXXFLAGS+=("-DSALAMANDER_LINK_WEBGPU=1")
  WGPU_PREFIX="${SALAMANDER_WGPU_PREFIX:-}"
  if [[ -z "$WGPU_PREFIX" ]]; then
    if [[ -d "/opt/homebrew/opt/wgpu-native" ]]; then
      WGPU_PREFIX="/opt/homebrew/opt/wgpu-native"
    elif [[ -d "/usr/local/opt/wgpu-native" ]]; then
      WGPU_PREFIX="/usr/local/opt/wgpu-native"
    fi
  fi

  if [[ -n "$WGPU_PREFIX" ]]; then
    if [[ -d "$WGPU_PREFIX/include" ]]; then
      EXTRA_CXXFLAGS+=("-I$WGPU_PREFIX/include")
    fi
    if [[ -d "$WGPU_PREFIX/lib" ]]; then
      EXTRA_CXXFLAGS+=("-L$WGPU_PREFIX/lib")
      EXTRA_LIBS+=("-Wl,-rpath,$WGPU_PREFIX/lib")
      if [[ -f "$WGPU_PREFIX/lib/libwgpu_native.dylib" || -f "$WGPU_PREFIX/lib/libwgpu_native.a" || -f "$WGPU_PREFIX/lib/libwgpu_native.so" ]]; then
        EXTRA_LIBS+=("-lwgpu_native")
      elif [[ -f "$WGPU_PREFIX/lib/libwebgpu.dylib" || -f "$WGPU_PREFIX/lib/libwebgpu.a" || -f "$WGPU_PREFIX/lib/libwebgpu.so" ]]; then
        EXTRA_LIBS+=("-lwebgpu")
      else
        EXTRA_LIBS+=("-lwgpu_native")
      fi
    else
      EXTRA_LIBS+=("-lwgpu_native")
    fi
  else
    EXTRA_LIBS+=("-lwgpu_native")
  fi
fi

clang++ -std=c++17 -fobjc-arc \
  -I/opt/homebrew/include -L/opt/homebrew/lib -I/usr/local/include -I"$ROOT_DIR" \
  -I"$ROOT_DIR/third_party/chuck/core" -I"$ROOT_DIR/third_party_vst/vst3sdk" \
  "${EXTRA_CXXFLAGS[@]:-}" \
  -o "$ROOT_DIR/cardinal" \
  "${EXTRA_SOURCES[@]:-}" \
  "$ROOT_DIR/third_party/chuck/build/libchuck.a" \
  "$ROOT_DIR/third_party_vst/vst3sdk/build/libvst3sdk_hosting.a" \
  -lglfw -ljack -lsndfile \
  "${EXTRA_LIBS[@]:-}" \
  -framework Cocoa -framework IOKit -framework CoreVideo
