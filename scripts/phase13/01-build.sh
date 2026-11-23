#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
trap 'status=$?; printf "Stage 01 failed at line %s (exit %s).\n" "$LINENO" "$status" >&2' ERR
source "$(dirname -- "${BASH_SOURCE[0]}")/stage-b-windows.sh"
stageBPreflight "$@"

mkdir -p -- "$stageBRunRoot"
# A failed build must never leave a previous run selected.
: > "$stageBRunRoot/latest"
stageBBuild=$(mktemp -d "$stageBRunRoot/run.XXXXXXXX")
mkdir -p -- "$stageBBuild/evidence"
stageBBuildWindows=$(cygpath -m "$stageBBuild")
stageBEvidenceWindows="$stageBBuildWindows/evidence"
printf 'Stage 01 evidence directory: %s\n' "$stageBEvidenceWindows"
git status --short --branch
printf 'Stage 01 source revision: %s\n' "$stageBRevision"

cmake -S "$stageBSourceWindows" -B "$stageBBuildWindows" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
   -DBUILD_TESTING=ON -DTASKCOMPASS_ENABLE_DB_TESTS=ON -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=ON \
   2>&1 | tee "$stageBBuild/evidence/configure.log"
cmake --build "$stageBBuildWindows" --parallel 2 2>&1 | tee "$stageBBuild/evidence/build.log"
stageBReport stage-b-native-ucrt64-build
[[ $(git rev-parse HEAD) == "$stageBRevision" && -z $(git status --porcelain) ]] \
   || stageBFail 'The source changed during the build. Rerun stage 01.'
printf '%s\n' "$stageBRevision" > "$stageBBuild/source-revision"
printf '%s\n' "${stageBBuild##*/}" > "$stageBRunRoot/latest"
printf 'Stage 01 passed. Run bash scripts/phase13/02-test.sh\n'
