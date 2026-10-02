#!/bin/bash

MAKE="make V=1 -j `nproc`"
src_dir=`realpath .`
src_dir="$src_dir"/
build_dir="$src_dir"build/x86_64

xserver_source="/usr/share/xorg-x11-server-source/"
xserver_patch="$src_dir/unix/xserver21.patch"

# clean build 
rm -rf build/unix
# FIXME: clean build argument?

mkdir -p build/unix

cp -R unix/xserver build/unix/
cp -R "$xserver_source"* build/unix/xserver/
pushd build/unix/xserver

patch -p1 < "$xserver_patch"
autoreconf -fiv

./configure --disable-xorg \
            --disable-xnest \
             --disable-xvfb \
             --disable-dmx \
             --disable-xwin \
             --disable-xephyr \
             --disable-kdrive \
             --disable-xwayland \
             --with-pic \
             --with-xkb-path=/usr/share/X11/xkb \
             --exec_prefix="" \
             --disable-static \
             --enable-glx \
             --disable-dri \
             --enable-dri2 \
             --enable-dri3 \
             --disable-unit-tests \
             --disable-config-hal \
             --disable-config-udev \
             --without-dtrace \
             --disable-devel-docs \
             --disable-selective-werror

$MAKE TIGERVNC_SRCDIR=$src_dir TIGERVNC_BUILDDIR=$build_dir

echo "Build complete. Binaries are in $build_dir"