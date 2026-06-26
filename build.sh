#!/usr/bin/env sh

set -e
cd "$(dirname "$0")" || exit 1

# add submodules
git submodule update --init

verbose=0
debug_bin="./build/linux/x86_64/debug/dotty"
release_bin="./build/linux/x86_64/release/dotty"
BIN=""

# portable nproc
if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=1
fi

# either debug or release
if [ "$1" = "dev" ]; then
    xmake config --show 2>/dev/null | grep -q '^mode=debug$' || xmake config -m debug
    BIN="$debug_bin"
    verbose="."
    shift
else
    xmake config --show 2>/dev/null | grep -q '^mode=release$' || xmake config -m release
    BIN="$release_bin"
    verbose=""
fi

xmake build -j"$JOBS" ${verbose:+-v} dotty
cp "$BIN" ./dotty
# ./dotty "$@"
