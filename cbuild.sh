#!/bin/bash
set -e

if [ -z "$1" ]; then
  echo "Error: No architecture selected"
  printf "\tUsage: %s [ armhf i386 osx64 win32 win64 x86_64 ]\n" "$0"
  exit 1
fi

MAKE="make V=1 -j`nproc`"
srcdir=$(dirname -- "$( readlink -f -- "$0")")
arch=$1

pwd=$srcdir/build/$arch

export CFLAGS="$CFLAGS -O2 -g -fdiagnostics-color -Wall"

# Uncomment for clean build
rm -rf "$pwd"

mkdir -p "$pwd"
pushd "$pwd"

# case $arch in
#   x86_64)
#     CMAKE='-G Unix Makefiles'
#     ;;

#   armhf)
#     CMAKE='-DCMAKE_TOOLCHAIN_FILE=/usr/share/cmake/Modules/toolchain/arm-none-linux-gnueabi'
#     ;;
#   win64)
#     CMAKE='-DCMAKE_TOOLCHAIN_FILE=/usr/share/cmake/Modules/toolchain/x86_64-w64-mingw32.cmake'
#     ;;
#   osx64)
#     CMAKE='-DCMAKE_TOOLCHAIN_FILE=/usr/share/cmake/Modules/toolchain/x86_64-apple-darwin10.cmake'
#     ;;
# esac


cmake "$CMAKE" \
                  -DCMAKE_BUILD_TYPE=Debug \
                  -DBUILD_STATIC=OFF \
                  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
                  -DENABLE_H264=OFF \
                  -S ../../
                  #-DCMAKE_CXX_FLAGS:STRING="$CFLAGS" \
                  #-DCMAKE_C_FLAGS:STRING="$CFLAGS" \
$MAKE

cp compile_commands.json ../../
