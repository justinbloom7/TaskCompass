#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Database-free build: explicitly turn off both live test registrations.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
buildDirectory="$repositoryDirectory/build"
if [[ -n "${MSYSTEM:-}" ]]; then
   [[ "$MSYSTEM" == UCRT64 ]] || { printf 'Use MSYS2 UCRT64.\n' >&2; exit 2; }
   export CMAKE_PREFIX_PATH
   CMAKE_PREFIX_PATH=$(cygpath -m /ucrt64)
   buildDirectory="$repositoryDirectory/build-ucrt64"
fi
if [[ $# -gt 0 ]]; then
   [[ $# == 2 && "$1" == --build-dir ]] || { printf 'Usage: bash scripts/phase12/01-build.sh [--build-dir PATH]\n' >&2; exit 2; }
   buildDirectory=$2
fi
cmake -S "$repositoryDirectory" -B "$buildDirectory" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
   -DTASKCOMPASS_ENABLE_DB_TESTS=OFF -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=OFF
cmake --build "$buildDirectory" --parallel 2
ctest --test-dir "$buildDirectory" --output-on-failure
