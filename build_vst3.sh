#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="$ROOT_DIR/third_party_vst/vst3sdk"
BUILD_DIR="$SDK_DIR/build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

rm -f *.o libvst3sdk_hosting.a

CXX="${CXX:-clang++}"
BUILD_MODE="${BUILD_MODE:-DEVELOPMENT}"
if [[ "$BUILD_MODE" == "RELEASE" ]]; then
  DEFINES="-DRELEASE=1 -DNDEBUG"
else
  DEFINES="-DDEVELOPMENT=1 -D_DEBUG"
fi
CXXFLAGS="-std=c++17 -O2 -I$SDK_DIR $DEFINES"

BASE_SOURCES=(
  "$SDK_DIR/base/source/baseiids.cpp"
  "$SDK_DIR/base/source/fbuffer.cpp"
  "$SDK_DIR/base/source/fdebug.cpp"
  "$SDK_DIR/base/source/fdynlib.cpp"
  "$SDK_DIR/base/source/fobject.cpp"
  "$SDK_DIR/base/source/fstreamer.cpp"
  "$SDK_DIR/base/source/fstring.cpp"
  "$SDK_DIR/base/source/timer.cpp"
  "$SDK_DIR/base/source/updatehandler.cpp"
  "$SDK_DIR/base/thread/source/fcondition.cpp"
  "$SDK_DIR/base/thread/source/flock.cpp"
)

PLUGINTERFACES_SOURCES=(
  "$SDK_DIR/pluginterfaces/base/conststringtable.cpp"
  "$SDK_DIR/pluginterfaces/base/coreiids.cpp"
  "$SDK_DIR/pluginterfaces/base/funknown.cpp"
  "$SDK_DIR/pluginterfaces/base/ustring.cpp"
)

COMMON_SOURCES=(
  "$SDK_DIR/public.sdk/source/common/commoniids.cpp"
  "$SDK_DIR/public.sdk/source/common/commonstringconvert.cpp"
  "$SDK_DIR/public.sdk/source/common/openurl.cpp"
  "$SDK_DIR/public.sdk/source/common/readfile.cpp"
  "$SDK_DIR/public.sdk/source/vst/vstpresetfile.cpp"
  "$SDK_DIR/public.sdk/source/vst/utility/stringconvert.cpp"
)

HOSTING_SOURCES=(
  "$SDK_DIR/public.sdk/source/vst/hosting/connectionproxy.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/eventlist.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/hostclasses.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/module.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/parameterchanges.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/pluginterfacesupport.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/plugprovider.cpp"
  "$SDK_DIR/public.sdk/source/vst/hosting/processdata.cpp"
  "$SDK_DIR/public.sdk/source/vst/vstinitiids.cpp"
)

OBJC_SOURCES=(
  "$SDK_DIR/public.sdk/source/common/threadchecker_mac.mm"
  "$SDK_DIR/public.sdk/source/common/systemclipboard_mac.mm"
  "$SDK_DIR/public.sdk/source/vst/hosting/module_mac.mm"
)

for src in "${BASE_SOURCES[@]}" "${PLUGINTERFACES_SOURCES[@]}" "${COMMON_SOURCES[@]}" "${HOSTING_SOURCES[@]}"; do
  $CXX $CXXFLAGS -c "$src"
done

for src in "${OBJC_SOURCES[@]}"; do
  $CXX $CXXFLAGS -fobjc-arc -c "$src"
done

ar rcs libvst3sdk_hosting.a *.o

echo "Built libvst3sdk_hosting.a in $BUILD_DIR"
