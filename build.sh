#!/usr/bin/env sh

set -e

debug_bin='./build/linux/x86_64/debug/dotty'
release_bin='./build/linux/x86_64/release/dotty'
copy_bin=""

# portable nproc
if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=1
fi

if [ "$1" = "rel" ]; then
    xmake config --show 2>/dev/null | grep -q mode=release || xmake config -m release
    copy_bin="$release_bin"
    shift
else
    xmake config --show 2>/dev/null | grep -q mode=debug || xmake config -m debug
    copy_bin="$debug_bin"
fi

xmake build -j"$JOBS" -v dotty || exit $?
command cp "$copy_bin" ./dotty
./dotty "$@"
