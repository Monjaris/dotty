#!/bin/bash

xmake build -j32 -v dotty || exit 1
command cp ./build/linux/x86_64/debug/dotty ./dotty
[[ "$1" == "c" ]] && clear && shift
./dotty "$@"
