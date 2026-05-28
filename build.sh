#!/bin/bash

if [[ "$1" == "rel" ]]; then
    [[ "$(xmake config --show 2>/dev/null | grep mode)" != *release* ]] && xmake config -m release
    shift
else
    [[ "$(xmake config --show 2>/dev/null | grep mode)" != *debug* ]] && xmake config -m debug
fi

xmake build -j$(nproc) -v dotty || exit $?
command cp ./build/linux/x86_64/debug/dotty ./dotty
./dotty "$@"
