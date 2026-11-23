#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail
trap 'status=$?; printf "Stage 02 failed at line %s (exit %s).\n" "$LINENO" "$status" >&2' ERR
source "$(dirname -- "${BASH_SOURCE[0]}")/stage-b-windows.sh"
stageBPreflight "$@"

[[ -s "$stageBRunRoot/latest" ]] || stageBFail 'Run stage 01 successfully first.'
IFS= read -r stageBRun < "$stageBRunRoot/latest"
[[ $stageBRun =~ ^run\.[[:alnum:]]{8}$ ]] || stageBFail 'Invalid build run selector. Rerun stage 01.'
stageBBuild="$stageBRunRoot/$stageBRun"
[[ -s "$stageBBuild/source-revision" && -f "$stageBBuild/CTestTestfile.cmake" ]] \
   || stageBFail 'The selected run is incomplete. Rerun stage 01.'
IFS= read -r stageBBuiltRevision < "$stageBBuild/source-revision"
[[ "$stageBBuiltRevision" == "$stageBRevision" ]] || stageBFail 'The source revision changed. Rerun stage 01.'
stageBBuildWindows=$(cygpath -m "$stageBBuild")
stageBEvidenceWindows="$stageBBuildWindows/evidence"
printf 'Stage 02 evidence directory: %s\n' "$stageBEvidenceWindows"

ctest --test-dir "$stageBBuildWindows" --show-only=json-v1 \
   > "$stageBBuild/evidence/ctest-inventory.json"
"$stageBPython" - "$stageBEvidenceWindows/ctest-inventory.json" <<'PYTHON'
import json
from pathlib import Path, PurePosixPath
import sys

tests = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))["tests"]
names = {test["name"] for test in tests}
commands = {
   PurePosixPath(test["command"][0].replace("\\", "/")).stem.casefold()
   for test in tests
   if test.get("command")
}
forbidden = {"mariadbintegrationtests", "organizationintegrationtests"}
blocked = forbidden & (commands | {name.casefold() for name in names})
if blocked:
   raise SystemExit("Live integration suite in CTest inventory: " + ", ".join(sorted(blocked)))
required = {"ContextStageTests", "OrganizationDesktopTests", "WorkspaceCodecTests", "DatabaseSchemaTests"}
if missing := required - names:
   raise SystemExit("Required offline suites absent: " + ", ".join(sorted(missing)))
print(f"{len(names)} CTest entries. Both live integration suites are absent.")
PYTHON

stageBReport stage-b-native-ucrt64-test
ctest --test-dir "$stageBBuildWindows" --output-on-failure \
   --output-log "$stageBEvidenceWindows/ctest.log"
[[ $(git rev-parse HEAD) == "$stageBRevision" && -z $(git status --porcelain) ]] \
   || stageBFail 'The source changed during testing. Rerun both stages.'
printf 'Stage 02 passed. Evidence directory: %s\n' "$stageBEvidenceWindows"
