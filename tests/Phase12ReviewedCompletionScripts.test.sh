#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_B75_INSPECTION_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12DevelopmentUpgradeScripts.test.sh"
# Shadow any installed rg: a future accidental call must fail this regression.
printf '#!/usr/bin/env bash\nprintf "Unexpected ripgrep dependency\\n" >&2\nexit 99\n' > "$temporaryDirectory/bin/rg"
chmod +x "$temporaryDirectory/bin/rg"
printf 'dev-client\ndev-grants-extra\n' > "$temporaryDirectory/count-fixture"
[[ "$(countDevMutations "$temporaryDirectory/count-fixture")" == 0 ]]
printf 'dev-grants\ndev-apply\ndev-runtime' > "$temporaryDirectory/count-fixture"
[[ "$(countDevMutations "$temporaryDirectory/count-fixture")" == 3 ]]
setup75 'reviewed completed schema2'
result=$(passwords75 | run75 02-migration-grants --approved 2>&1)
# Fake the exact observed completed-runner/pre-verification state, not live SQL.
printf '2\n' > "$DEV_SCHEMA"
cp "$snapshot/b75-grants-success.txt" "$snapshot/b75-migration-attempt.txt"
export DEV_TASK_INDEX_V2=1
mutations=$(countDevMutations "$MOCK_DIRECTORY/calls")
result=$(passwords75 | run75 07-complete-reviewed-verification --approved 2>&1)
[[ "$result" == *'PASS: reviewed Linux completed-migration preservation'* && -s "$snapshot/b75-reviewed-completion.sha256" && -s "$snapshot/b75-preservation.sha256" && "$mutations" == "$(countDevMutations "$MOCK_DIRECTORY/calls")" ]]
if result=$(passwords75 | run75 07-complete-reviewed-verification --approved 2>&1); then exit 1; fi
result=$(passwords75 | run75 04-runtime-grants --approved 2>&1)
result=$(passwords75 | run75 05-verify --approved 2>&1)
[[ "$result" == *'PASS: B7.5 development upgrade/final privileges/preservation/health'* ]]
unset DEV_TASK_INDEX_V2
for mode in DEV_DDL_DRIFT DEV_BAD_JOURNAL DEV_BAD_HEALTH DEV_LEGACY_DRIFT; do
 setup75 "$mode completion refused"
 result=$(passwords75 | run75 02-migration-grants --approved 2>&1)
 printf '2\n' > "$DEV_SCHEMA"
 cp "$snapshot/b75-grants-success.txt" "$snapshot/b75-migration-attempt.txt"
 export "$mode=1"
 if result=$(passwords75 | run75 07-complete-reviewed-verification --approved 2>&1); then exit 1; fi
 unset "$mode"
 [[ -s "$snapshot/b75-migration-attempt.txt" && ! -e "$snapshot/b75-migration-success.txt" && "$(cat "$DEV_POLICY")" == migration ]]
done
printf 'PASS: verification-only completion accepts reviewed implicit index alias, never applies/grants; unrelated DDL/data/journal/health deviations refuse completion.\n'
