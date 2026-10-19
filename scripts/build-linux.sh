#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
command -v cmake >/dev/null || { echo "Install CMake 3.22 or newer." >&2; exit 1; }
command -v ctest >/dev/null || { echo "Install the complete CMake tools, including CTest." >&2; exit 1; }
configure=(-S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON)
if [[ -n "${1:-}" ]]; then
  configure+=("-DIDW_JUCE_PATH=$1")
fi
cmake "${configure[@]}"
cmake --build build-linux --config Release --parallel 2
ctest --test-dir build-linux -C Release --output-on-failure --no-tests=error
echo "Native build and registered tests passed. Host, installer and hardware validation remain required."
