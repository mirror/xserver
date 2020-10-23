#!/bin/bash

set -e
set -o xtrace

meson build/
meson dist --no-tests -C build/

cd build
tar xf meson-dist/xwayland-*.tar.xz
cd xwayland-*/

meson -Dc_args="-fno-common" -Dprefix=/usr -Dwerror=true build/
DESTDIR=$PWD/install/ ninja -j${FDO_CI_CONCURRENT:-4} -C build/ install
