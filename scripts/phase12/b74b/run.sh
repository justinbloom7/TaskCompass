#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Approved operator-only runtime rehearsal. Never targets development databases.
set +x
set -Eeuo pipefail
b74bAction=${1:-help}
[[ $# == 0 ]] || shift
case "$b74bAction" in preflight|grants|probe) ;; *) printf 'B7.4b: 01-preflight.sh, 02-runtime-grants.sh, 03-probe.sh (each --approved).\n'; exit 0 ;; esac
for option in "$@"; do
   case "$option" in --help|-h|--print-plan) printf 'B7.4b: fixed schema2 clone only; exact runtime grants then rollback-only probe; no dev changes/reset/repair.\n'; exit 0 ;; esac
done
b74bDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TASKCOMPASS_B74_LIBRARY=1 source "$b74bDirectory/../b74a/run.sh" preflight "$@"
trap 'fail "B7.4b operation failed (exit $?)."' ERR
[[ "${TASKCOMPASS_B74B_LIBRARY:-0}" == 1 ]] || printf 'B7.4b stage: %s\n' "$b74bAction"
readonly RUNTIME_HASH=9af378bc61c18a87bf76bb03f9472fc149ed28ec2914f74eac86ee727937bb7e
readonly PROBE_HASH=0fa9b45a4f14dc9ffb2063915e0d5e5d35f95015ed64beaa2442c36dcc718417
checkManifest() {
   local manifest=$1 expected=$2 line name names=''
   privateFile "$manifest"
   while IFS= read -r line || [[ -n "$line" ]]; do
      line=${line%$'\r'}
      [[ "$line" =~ ^[0-9a-fA-F]{64}\ [\ \*](.+)$ ]] || fail 'Malformed evidence checksum.'
      name=${BASH_REMATCH[1]}
      [[ "$name" != *[![:alnum:]._-]* && " $expected " == *" $name "* && " $names " != *" $name "* ]] || fail 'Unsafe/duplicate evidence path.'
      privateFile "$name"; names+="$name "
   done < "$backupDirectory/$manifest"
   [[ "$(printf '%s\n' $names | LC_ALL=C sort)" == "$(printf '%s\n' $expected | LC_ALL=C sort)" ]] || fail 'Incomplete evidence manifest.'
   (cd -- "$backupDirectory" && sha256sum --check "$manifest")
}
# Reuse definitions only; no rehearsal reads, writes or live operations.
[[ "${TASKCOMPASS_B74B_LIBRARY:-0}" != 1 ]] || return 0
snapshot
checkRehearsalEvidence
checkSql
checkManifest b74a-preservation.sha256 'rehearsal2-legacy.hex.tsv rehearsal2.ddl.txt b74a-upgrade-checks.tsv b74a-health.tsv b74a-grants-attempt.txt b74a-grants-success.txt b74a-migration-attempt.txt b74a-migration-success.txt'
for name in b74a-grants-attempt.txt b74a-grants-success.txt b74a-migration-attempt.txt b74a-migration-success.txt; do marker "$name"; done
for pair in "rehearsal-runtime-grants.sql $RUNTIME_HASH" "runtime-probe.sql $PROBE_HASH"; do
   read -r name expected <<< "$pair"
   digest=$(sha256sum < "scripts/phase12/b7/$name")
   [[ "${digest%% *}" == "$expected" ]] || fail 'Runtime SQL differs from approval.'
done
receipt="$receipt $RUNTIME_HASH $PROBE_HASH"
baseline2() {
   runVerify 2
   local sql sealed current expected
   for pair in 'legacy-export.sql rehearsal2-legacy.hex.tsv' 'ddl2-export.sql rehearsal2.ddl.txt' 'upgrade-postconditions.sql b74a-upgrade-checks.tsv' 'schema2-health.sql b74a-health.tsv'; do
      read -r sql sealed <<< "$pair"
      current=$(query < "scripts/phase12/b7/$sql" | sha256sum)
      expected=$(sha256sum < "$backupDirectory/$sealed")
      [[ "${current%% *}" == "${expected%% *}" ]] || fail 'Schema2 legacy/DDL/journal/health changed since B7.4a.'
   done
}
fullMatches() {
   local digest expected
   digest=$(query < scripts/phase12/b7/schema2-export.sql | sha256sum)
   expected=$(sha256sum < "$backupDirectory/b74b-before-grants.hex.tsv")
   [[ "${digest%% *}" == "${expected%% *}" ]] || fail 'Nine-table data changed.'
}
if [[ "$b74bAction" == preflight ]]; then
   newFiles b74b-grants-attempt.txt b74b-grants-success.txt b74b-before-grants.hex.tsv
   baseline2
   adminStart; preflight narrow 2; adminEnd
   printf 'PASS: B7.4b fresh schema2/evidence/narrow-role baseline.\n'
   exit 0
fi
if [[ "$b74bAction" == grants ]]; then
   newFiles b74b-grants-attempt.txt b74b-grants-success.txt b74b-before-grants.hex.tsv b74b-grants.sha256
   confirm 'RUNTIME GRANTS taskcompass_phase12_test'
   confirm 'WRITERS CLOSED; BASELINE UNCHANGED'
   baseline2
   adminStart; preflight narrow 2
   baseline2
   (set -o noclobber; query < scripts/phase12/b7/schema2-export.sql > "$backupDirectory/b74b-before-grants.hex.tsv")
   writeMarker b74b-grants-attempt.txt
   adminQuery scripts/phase12/b7/rehearsal-runtime-grants.sql
   [[ -z "$adminReport" ]] || fail 'Unexpected runtime grant result.'
   preflight runtime 2
   baseline2; fullMatches
   adminEnd
   writeMarker b74b-grants-success.txt
   (cd -- "$backupDirectory" && set -o noclobber && sha256sum b74b-before-grants.hex.tsv b74b-grants-attempt.txt b74b-grants-success.txt > b74b-grants.sha256)
   printf 'PASS: exact19 runtime table privileges; no schema rights; schema2/data unchanged. Probe still pending.\n'
   exit 0
fi
checkManifest b74b-grants.sha256 'b74b-before-grants.hex.tsv b74b-grants-attempt.txt b74b-grants-success.txt'
marker b74b-grants-attempt.txt; marker b74b-grants-success.txt
newFiles b74b-probe-attempt.txt b74b-probe-success.txt rehearsal-before-probe.hex.tsv rehearsal-after-probe.hex.tsv b74b-probe-result.tsv b74b-probe.sha256
confirm 'ROLLBACK PROBE taskcompass_phase12_test'
confirm 'WRITERS CLOSED; BASELINE UNCHANGED'
baseline2
adminStart; preflight runtime 2; adminEnd
baseline2; fullMatches
(set -o noclobber; query < scripts/phase12/b7/schema2-export.sql > "$backupDirectory/rehearsal-before-probe.hex.tsv")
sameFiles "$backupDirectory/b74b-before-grants.hex.tsv" "$backupDirectory/rehearsal-before-probe.hex.tsv"
writeMarker b74b-probe-attempt.txt
(set -o noclobber; query < scripts/phase12/b7/runtime-probe.sql > "$backupDirectory/b74b-probe-result.tsv")
expected=$'1\n1\n1\n1\nPillar\t0\nProject\t0\nObjective\t0\nTask\t0\nCommandRecord\t0\nLocalChange\t0'
actual=$(cat "$backupDirectory/b74b-probe-result.tsv"); actual=${actual//$'\r'/}
[[ "$actual" == "$expected" ]] || fail 'Probe counts differ; preserve uncertain state.'
(set -o noclobber; query < scripts/phase12/b7/schema2-export.sql > "$backupDirectory/rehearsal-after-probe.hex.tsv")
sameFiles "$backupDirectory/rehearsal-before-probe.hex.tsv" "$backupDirectory/rehearsal-after-probe.hex.tsv"
baseline2
writeMarker b74b-probe-success.txt
(cd -- "$backupDirectory" && set -o noclobber && sha256sum rehearsal-before-probe.hex.tsv rehearsal-after-probe.hex.tsv b74b-probe-result.tsv b74b-probe-attempt.txt b74b-probe-success.txt > b74b-probe.sha256)
(cd -- "$backupDirectory" && sha256sum --check b74b-probe.sha256)
printf 'PASS: B7.4b runtime verification and rollback-only probe; all nine tables unchanged.\nSTOP: B7.5 development migration requires separate approval.\n'
