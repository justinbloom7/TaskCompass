#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
trap 'status=$?; printf "Stage 04 failed at line %s (exit %s).\n" "$LINENO" "$status" >&2' ERR
source "$(dirname -- "${BASH_SOURCE[0]}")/stage-b-windows.sh"
stageBPreflight "$@"
stageBRunRoot="$stageBRepository/build-ucrt64-stage-c"

[[ -s "$stageBRunRoot/latest" ]] || stageBFail 'Run stage 03 successfully first.'
IFS= read -r stageBRun < "$stageBRunRoot/latest"
[[ $stageBRun =~ ^run\.[[:alnum:]]{8}$ ]] || stageBFail 'Invalid build run selector. Rerun stage 03.'
stageBBuild="$stageBRunRoot/$stageBRun"
[[ -s "$stageBBuild/source-revision" && -f "$stageBBuild/CTestTestfile.cmake" ]] \
   || stageBFail 'The selected run is incomplete. Rerun stage 03.'
IFS= read -r stageBBuiltRevision < "$stageBBuild/source-revision"
[[ "$stageBBuiltRevision" == "$stageBRevision" ]] || stageBFail 'The source revision changed. Rerun stage 03.'
stageBBuildWindows=$(cygpath -m "$stageBBuild")
stageBEvidenceWindows="$stageBBuildWindows/evidence"
printf 'Stage 04 evidence directory: %s\n' "$stageBEvidenceWindows"

ctest --test-dir "$stageBBuildWindows" --show-only=json-v1 \
   > "$stageBBuild/evidence/ctest-inventory.json"
"$stageBPython" "$stageBSourceWindows/scripts/phase13/check-stage-c-inventory.py" \
   "$stageBEvidenceWindows/ctest-inventory.json"

stageBReport stage-c-native-ucrt64-test
ctest --test-dir "$stageBBuildWindows" --output-on-failure \
   --output-log "$stageBEvidenceWindows/ctest.log"
[[ $(git rev-parse HEAD) == "$stageBRevision" && -z $(git status --porcelain) ]] \
   || stageBFail 'The source changed during testing. Rerun both stages.'
printf 'Stage 04 passed. Evidence directory: %s\n' "$stageBEvidenceWindows"
