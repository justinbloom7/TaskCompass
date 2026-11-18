#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Linux-only reviewed completed-migration verification. No live mutation SQL.
set +x
set -Eeuo pipefail
for option in "$@"; do
   case "$option" in --help|-h|--print-plan) printf 'Linux-only reviewed schema2 verification/evidence completion; no apply/grants/repair.\n'; exit 0 ;; esac
done
completionScriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TASKCOMPASS_B75_LIBRARY=1 source "$completionScriptDirectory/run.sh" preflight "$@"
[[ "$platform" == linux ]] || fail 'This reviewed completion is Linux-only.'
trap 'fail "Reviewed verification failed (exit $?)."' ERR
checkManifest b75-grants.sha256 'b75-grants-attempt.txt b75-grants-success.txt b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt'
marker b75-grants-attempt.txt; marker b75-grants-success.txt
marker b75-migration-attempt.txt
newFiles b75-migration-success.txt b75-preservation.sha256 b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv b75-reviewed-completion.txt b75-reviewed-completion.sha256
# Completed journal and registered structure required; partial migrations refuse here.
baseline75 2
adminStart; preflight75 migration 2; adminEnd
baseline75 2
export75 legacy-export.sql b75-legacy.hex.tsv
export75 ddl2-export.sql b75-schema2.ddl.txt
export75 upgrade-postconditions.sql b75-upgrade-checks.tsv
export75 schema2-health.sql b75-health.tsv
export75 schema2-export.sql b75-schema2.hex.tsv
sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/b75-legacy.hex.tsv"
bash "$b75Directory/compare-schema2-ddl.sh" "$backupDirectory/rehearsal2.ddl.txt" "$backupDirectory/b75-schema2.ddl.txt"
checkPost75; checkHealth75 b75-health.tsv
writeMarker b75-reviewed-completion.txt
writeMarker b75-migration-success.txt
seal75 b75-preservation.sha256 b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv
seal75 b75-reviewed-completion.sha256 b75-reviewed-completion.txt b75-preservation.sha256
printf 'PASS: reviewed Linux completed-migration preservation verified; original attempt retained; no migration or grants replayed.\nSTOP: report this result before continuing runtime transition.\n'
