#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only diagnosis of a runner-verified schema2 preservation failure.
set +x
set -Eeuo pipefail
for option in "$@"; do
   case "$option" in --help|-h|--print-plan)
      printf 'Read-only schema2 failure inspection; fresh private exports only. No grants/apply/repair or stage replay.\n'; exit 0 ;;
   esac
done
inspectionScriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TASKCOMPASS_B75_LIBRARY=1 source "$inspectionScriptDirectory/run.sh" preflight "$@"
trap 'fail "Read-only B7.5 inspection failed (exit $?)."' ERR
checkManifest b75-grants.sha256 'b75-grants-attempt.txt b75-grants-success.txt b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt'
marker b75-grants-attempt.txt; marker b75-grants-success.txt
marker b75-migration-attempt.txt
verify75 2
authenticate taskcompass_dev
adminStart; preflight75 migration 2; adminEnd
inspectionDirectory="$backupDirectory/b75-inspection-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -- "$inspectionDirectory"
printf 'Private inspection directory: %s\n' "$inspectionDirectory"
# Parent capture ACL/mode was verified by snapshot(); child inherits private ACL.
for pair in 'legacy-export.sql legacy-current.hex.tsv' 'ddl2-export.sql schema2-current.ddl.txt' 'upgrade-postconditions.sql postconditions.tsv' 'schema2-health.sql health.tsv'; do
   read -r sql destination <<< "$pair"
   (set -o noclobber; query75 taskcompass_dev < "scripts/phase12/b7/$sql" > "$inspectionDirectory/$destination")
done
for pair in 'legacy-before.hex.tsv legacy-current.hex.tsv' 'rehearsal2.ddl.txt schema2-current.ddl.txt'; do
   read -r baseline current <<< "$pair"
   expected=$(sha256sum < "$backupDirectory/$baseline")
   actual=$(sha256sum < "$inspectionDirectory/$current")
   printf '%s: expected %s; actual %s\n' "$current" "${expected%% *}" "${actual%% *}"
   if [[ "${expected%% *}" == "${actual%% *}" ]]; then
      printf 'MATCH: %s\n' "$current"
   else
      printf 'DIFFERENT: %s; preserve for review.\n' "$current"
   fi
done
# DDL contains schema definitions, never task titles/notes/history values.
if ! command -v diff >/dev/null 2>&1; then
   printf 'diff is unavailable; compare the two private DDL paths shown above.\n' > "$inspectionDirectory/schema2.diff"
   cat "$inspectionDirectory/schema2.diff"
elif diff -u -- "$backupDirectory/rehearsal2.ddl.txt" "$inspectionDirectory/schema2-current.ddl.txt" > "$inspectionDirectory/schema2.diff"; then
   printf 'DDL diff: empty.\n'
else
   status=$?
   [[ "$status" == 1 ]] || fail 'DDL diff command failed.'
   cat "$inspectionDirectory/schema2.diff"
fi
(cd -- "$inspectionDirectory" && set -o noclobber && sha256sum legacy-current.hex.tsv schema2-current.ddl.txt postconditions.tsv health.tsv schema2.diff > inspection.sha256)
(cd -- "$inspectionDirectory" && sha256sum --check inspection.sha256)
printf 'Inspection captured only; no completion receipt, grant or migration was executed.\nSTOP: report output; do not run stage03/04 or change markers.\n'
