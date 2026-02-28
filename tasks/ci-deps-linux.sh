#!/bin/sh
# Install Linux build dependencies for CI.
# Called by GitHub Actions workflows; not intended for local use.
#
# Usage:
#   sh tasks/ci-deps-linux.sh                         # base deps only
#   sh tasks/ci-deps-linux.sh clang clang-tidy        # base + extra packages
#   sh tasks/ci-deps-linux.sh clang llvm              # base + coverage tools
set -eu

sudo apt-get update
sudo apt-get install -y \
  ninja-build pkg-config \
  libwebkit2gtk-4.1-dev libvulkan-dev libfreetype-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
  libxfixes-dev libxi-dev libxss-dev \
  libwayland-dev libxkbcommon-dev libdrm-dev libgbm-dev \
  libgl-dev libegl-dev \
  libasound2-dev libpulse-dev libpipewire-0.3-dev \
  libdbus-1-dev libudev-dev \
  "$@"
