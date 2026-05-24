#!/bin/bash

if [[ "$1" == "rel" ]]; then
    xmake config --cxflags="-DDEBUG_ON=0"
    xmake build -j$(nproc) --rebuild -v dotty || exit $?
    shift
else
    xmake config --cxflags=""
    xmake build -j$(nproc) -v dotty || exit $?
fi

command cp ./build/linux/x86_64/debug/dotty ./dotty
./dotty "$@"
