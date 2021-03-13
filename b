#!/bin/bash
set -e
if [ ! -d "build" ]; then
  meson build -Dcava=disabled
fi
ninja -C build
