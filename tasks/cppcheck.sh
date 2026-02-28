#!/bin/sh
# Run cppcheck static analysis on Fabric source files.
set -eu

if ! command -v cppcheck >/dev/null 2>&1; then
  echo "cppcheck not found. Install via: brew install cppcheck" >&2
  exit 1
fi

echo "Running cppcheck"
cppcheck \
  --enable=warning,performance,portability \
  --error-exitcode=1 \
  --suppress=missingInclude \
  --suppress=unmatchedSuppression \
  --suppress=syntaxError:include/fabric/core/DebugDraw.hh \
  --suppress=uninitMemberVar:src/core/ParticleSystem.cc \
  --suppress=invalidPointerCast:src/core/ParticleSystem.cc \
  -I include/ \
  src/ include/
