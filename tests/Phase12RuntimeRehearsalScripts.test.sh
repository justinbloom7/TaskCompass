#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_B74_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12MigrationRehearsalScripts.test.sh"
prepare() {
   helperDirectory="$repositoryDirectory/scripts/phase12/b74a"
   setup "$1"
   result=$(grantsInput | run 02-grants --approved 2>&1)
   result=$(migrateInput | run 03-migrate --approved 2>&1)
   result=$(printf '%s\n' "$MOCK_SECRET" | run 04-preservation --approved 2>&1)
   helperDirectory="$repositoryDirectory/scripts/phase12/b74b"
}
runtimeInput() { printf 'RUNTIME GRANTS taskcompass_phase12_test\nWRITERS CLOSED; BASELINE UNCHANGED\n%s\n' "$MOCK_SECRET"; }
probeInput() { printf 'ROLLBACK PROBE taskcompass_phase12_test\nWRITERS CLOSED; BASELINE UNCHANGED\n%s\n' "$MOCK_SECRET"; }
for platform in linux windows; do
   unset MSYSTEM
   [[ "$platform" != windows ]] || export MSYSTEM=UCRT64
   prepare "$platform runtime"
   for stage in 01-preflight 02-runtime-grants 03-probe; do
      run "$stage" --print-plan >/dev/null
      if result=$(run "$stage" 2>&1); then exit 1; fi
   done
   result=$(printf '%s\n' "$MOCK_SECRET" | run 01-preflight --approved 2>&1)
   [[ "$result" == *'PASS: B7.4b fresh'* ]]
   result=$(runtimeInput | run 02-runtime-grants --approved 2>&1)
   [[ "$result" == *'PASS: exact19 runtime'* && "$result" != *"$MOCK_SECRET"* ]]
   if result=$(runtimeInput | run 02-runtime-grants --approved 2>&1); then exit 1; fi
   result=$(probeInput | run 03-probe --approved 2>&1)
   [[ "$result" == *'PASS: B7.4b runtime verification'* ]]
   if result=$(probeInput | run 03-probe --approved 2>&1); then exit 1; fi
   printf 'PASS: %s mocked runtime19 grants, Qt verification, rollback counts and nine-table equality; no replay.\n' "$platform"
done
unset MSYSTEM
for failureMode in MOCK_RUNTIME_GRANT_FAIL MOCK_RUNTIME_EXTRA MOCK_LEGACY_DRIFT; do
   prepare "negative $failureMode"
   export "$failureMode=1"
   if result=$(runtimeInput | run 02-runtime-grants --approved 2>&1); then exit 1; fi
   unset "$failureMode"
   [[ ! -e "$snapshot/b74b-grants-success.txt" ]]
done
for failureMode in MOCK_PROBE_FAIL MOCK_PROBE_DRIFT MOCK_PROBE_COUNT; do
   prepare "negative $failureMode"
   result=$(runtimeInput | run 02-runtime-grants --approved 2>&1)
   export "$failureMode=2"
   [[ "$failureMode" == MOCK_PROBE_COUNT ]] || export "$failureMode=1"
   if result=$(probeInput | run 03-probe --approved 2>&1); then exit 1; fi
   unset "$failureMode"
   [[ -s "$snapshot/b74b-probe-attempt.txt" && ! -e "$snapshot/b74b-probe-success.txt" ]]
   if result=$(probeInput | run 03-probe --approved 2>&1); then exit 1; fi
done
printf 'PASS: partial/extra grants, baseline drift, failed probe/bad counts/data drift stop without success or replay.\n'
