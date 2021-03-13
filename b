#!/bin/bash
set -e
if [ ! -d "build" ]; then
  meson build
fi
ninja -C build
