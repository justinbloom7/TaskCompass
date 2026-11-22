#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Preparation package; live use requires separate approval for this platform/stage.
set +x
set -Eeuo pipefail
b75Action=${1:-help}
[[ $# == 0 ]] || shift
usage75() {
   printf '%s\n' 'B7.5: 01-preflight, 02-migration-grants, 03-migrate, 04-runtime-grants, 05-verify.'
   printf '%s\n' 'Each: --approved --platform linux|windows (separate platform authorization required).'
   printf '%s\n' 'Only fixed taskcompass_dev; no typed confirmations, reset, repair, resume or GUI.'
}
case "$b75Action" in preflight|grants|migrate|runtime|verify) ;; help) usage75; exit 0 ;; *) usage75 >&2; exit 1 ;; esac
selectedPlatform=''
forwardOptions=()
while [[ $# -gt 0 ]]; do
   case "$1" in
      --help|-h|--print-plan) usage75; exit 0 ;;
      --platform) [[ $# -ge 2 ]] || exit 1; selectedPlatform=$2; shift 2 ;;
      *) forwardOptions+=("$1"); shift ;;
   esac
done
case "$selectedPlatform:${MSYSTEM:-}" in linux:|windows:UCRT64) ;; *) printf 'STOP: explicit --platform must match Linux Bash or Windows UCRT64.\n' >&2; exit 1 ;; esac
b75Directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TASKCOMPASS_B74B_LIBRARY=1 TASKCOMPASS_B74_CREDENTIAL_DATABASE=taskcompass_dev \
   source "$b75Directory/../b74b/run.sh" preflight "${forwardOptions[@]}"
trap 'fail "B7.5 operation failed (exit $?)."' ERR
trap 'fail "B7.5 interrupted; outcome may be partial."' INT TERM
if [[ "$platform" == windows ]]; then
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}TASKCOMPASS_DB_PASSWORD"
fi
[[ "${TASKCOMPASS_B75_LIBRARY:-0}" == 1 ]] || printf 'B7.5 %s: %s (taskcompass_dev only)\n' "$platform" "$b75Action"
printf 'Required precondition: all TaskCompass writers stopped; automatic session checks will run.\n'
readonly DEV_GRANT_HASH=517aaef4966eb3a50f0ec6f0c6f1b66f540ba474043ff54a46f5c4ed8030d4fb
readonly DEV_RUNTIME_HASH=c474535e2d9af43ba570f6d0694936a59d01e6e2b8bab37a779f5cce10e04442
snapshot
checkRehearsalEvidence
checkSql
checkManifest b74a-preservation.sha256 'rehearsal2-legacy.hex.tsv rehearsal2.ddl.txt b74a-upgrade-checks.tsv b74a-health.tsv b74a-grants-attempt.txt b74a-grants-success.txt b74a-migration-attempt.txt b74a-migration-success.txt'
for name in b74a-grants-attempt.txt b74a-grants-success.txt b74a-migration-attempt.txt b74a-migration-success.txt; do marker "$name"; done
receipt="$receipt $RUNTIME_HASH $PROBE_HASH"
checkManifest b74b-grants.sha256 'b74b-before-grants.hex.tsv b74b-grants-attempt.txt b74b-grants-success.txt'
checkManifest b74b-probe.sha256 'rehearsal-before-probe.hex.tsv rehearsal-after-probe.hex.tsv b74b-probe-result.tsv b74b-probe-attempt.txt b74b-probe-success.txt'
for name in b74b-grants-attempt.txt b74b-grants-success.txt b74b-probe-attempt.txt b74b-probe-success.txt; do marker "$name"; done
sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/rehearsal2-legacy.hex.tsv"
sameFiles "$backupDirectory/b74b-before-grants.hex.tsv" "$backupDirectory/rehearsal-before-probe.hex.tsv"
sameFiles "$backupDirectory/rehearsal-before-probe.hex.tsv" "$backupDirectory/rehearsal-after-probe.hex.tsv"
expectedProbe=$'1\n1\n1\n1\nPillar\t0\nProject\t0\nObjective\t0\nTask\t0\nCommandRecord\t0\nLocalChange\t0'
actualProbe=$(cat "$backupDirectory/b74b-probe-result.tsv"); actualProbe=${actualProbe//$'\r'/}
[[ "$actualProbe" == "$expectedProbe" ]] || fail 'Retained runtime rehearsal counts differ.'
for pair in "migration-dev-grants.sql $DEV_GRANT_HASH" "runtime-dev-grants-and-revoke.sql $DEV_RUNTIME_HASH"; do
   read -r name expected <<< "$pair"
   digest=$(sha256sum < "scripts/phase12/b7/$name")
   [[ "${digest%% *}" == "$expected" ]] || fail 'Development grant SQL differs from review.'
done
receipt="$receipt $DEV_GRANT_HASH $DEV_RUNTIME_HASH $platform taskcompass_dev"
query75() {
   local user=$1; shift
   authenticate "$user" >&2
   MYSQL_PWD="${passwords[$user]}" "$clientPath" --no-defaults --skip-reconnect --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user="$user" --connect-timeout=5 \
      --default-character-set=utf8mb4 --database=taskcompass_dev --batch --raw --skip-column-names "$@" 2> >(redact)
}
verify75() {
   local version=$1 user=${2:-taskcompass_migrate} output
   authenticate "$user"
   output=$(TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306 TASKCOMPASS_DB_NAME=taskcompass_dev \
      TASKCOMPASS_DB_USER="$user" TASKCOMPASS_DB_ISOLATED=1 TASKCOMPASS_DB_PASSWORD="${passwords[$user]}" \
      "$executable" --verify --to-version "$version" 2> >(redact)) || fail 'Development schema verification failed.'
   output=${output//$'\r'/}; printf '%s\n' "$output"
   [[ "$output" == "Schema $version verified; dataset $expectedDataset" ]] || fail 'Unexpected development schema/UUID.'
}
baseline75() {
   local version=$1 digest expected sql sealed
   verify75 "$version"
   authenticate taskcompass_dev
   for pair in 'legacy-export.sql legacy-before.hex.tsv' "ddl$version-export.sql schema1.ddl.txt"; do
      read -r sql sealed <<< "$pair"
      [[ "$version:$sql" != 2:ddl2-export.sql ]] || sealed=rehearsal2.ddl.txt
      if [[ "$version:$sql" == 2:ddl2-export.sql ]]; then
         query75 taskcompass_dev < "scripts/phase12/b7/$sql" | bash "$b75Directory/compare-schema2-ddl.sh" "$backupDirectory/$sealed" - || fail 'Development comparison failed: ddl2-export.sql versus rehearsal2.ddl.txt.'
         continue
      fi
      digest=$(query75 taskcompass_dev < "scripts/phase12/b7/$sql" | sha256sum)
      expected=$(sha256sum < "$backupDirectory/$sealed")
      [[ "${digest%% *}" == "${expected%% *}" ]] || fail "Development comparison failed: $sql versus $sealed (expected ${expected%% *}, actual ${digest%% *})."
   done
}
# Compare complete metadata, not grant counts. Reject rights on any other scope.
preflight75() {
   local policy=$1 version=$2 line tag values account data publicGrants='' metadata expectedObjects='' objects='' name right
   local -A seen=() schemaRights=() tableRights=()
   adminQuery "$b75Directory/preflight.sql"; metadata=$adminReport
   if [[ "$platform" == windows ]]; then
      adminQuery scripts/phase12/b7/public-grants.sql; publicGrants=$adminReport
   fi
   while IFS= read -r line; do
      [[ -n "$line" ]] || continue
      tag=${line%%$'\t'*}; values=${line#*$'\t'}
      case "$tag" in
         SERVER|STATE) [[ ! -v "seen[$tag]" ]] || fail 'Duplicate development metadata.'; seen[$tag]=$values ;;
         OBJECT) objects+="$values"$'\n' ;;
         HOST|GLOBAL|COLUMN|ROLE|RIGHT|TABLE_RIGHT)
            account=${values%%$'\t'*}; data=${values#*$'\t'}
            case "$account" in taskcompass_dev|taskcompass_migrate|taskcompass_app) ;; *) fail 'Unexpected account metadata.' ;; esac
            case "$tag" in
               RIGHT) schemaRights[$account]+="$data"$'\n' ;;
               TABLE_RIGHT) tableRights[$account]+="$data"$'\n' ;;
               *) name="$tag:$account"; [[ ! -v "seen[$name]" ]] || fail 'Duplicate/extra account host.'; seen[$name]=$data ;;
            esac ;;
         *) fail 'Unexpected development metadata output.' ;;
      esac
   done <<< "$metadata"
   [[ "${seen[SERVER]:-}" == "$expectedHost"$'\t'"$expectedVersion"$'\t3306\ttaskcompass_dev\troot@localhost\t'"$expectedTableCase"$'\t1\t1' ]] || fail 'Wrong dev server/DB/root/session identity.'
   [[ "${seen[STATE]:-}" == $'1\t0\t0\t0\t0' ]] || fail 'Unexpected encoding/stored code/active writers or sessions.'
   for name in commandrecord dataset localchange schemamigration task; do expectedObjects+="$name"$'\tBASE TABLE\tInnoDB\n'; done
   if [[ "$version" == 2 ]]; then
      for name in pillar project objective migrationattempt; do expectedObjects+="$name"$'\tBASE TABLE\tInnoDB\n'; done
   fi
   [[ "$(printf '%s' "$objects" | LC_ALL=C sort)" == "$(printf '%s' "$expectedObjects" | LC_ALL=C sort)" ]] || fail 'Unexpected development objects/engines.'
   for account in taskcompass_dev taskcompass_migrate taskcompass_app; do
      [[ "${seen[HOST:$account]:-}" == 127.0.0.1 ]] || fail 'Missing/unexpected development account host.'
      for tag in GLOBAL COLUMN ROLE; do [[ "${seen[$tag:$account]:-}" == 0 ]] || fail 'Unexpected global/column/role authority.'; done
      local expectedSchema='' expectedTables=''
      if [[ "$account" != taskcompass_app ]]; then expectedSchema=$'taskcompass\\_dev\tSELECT\tNO\n'; fi
      if [[ "$account" == taskcompass_app ]]; then
         for right in SELECT INSERT UPDATE; do expectedTables+=$'taskcompass_dev\ttask\t'"$right"$'\tNO\n'; done
         for name in commandrecord localchange; do
            for right in SELECT INSERT; do expectedTables+=$'taskcompass_dev\t'"$name"$'\t'"$right"$'\tNO\n'; done
         done
         for name in dataset schemamigration; do expectedTables+=$'taskcompass_dev\t'"$name"$'\tSELECT\tNO\n'; done
         if [[ "$policy" == final ]]; then
            for name in pillar project objective; do
               for right in SELECT INSERT UPDATE; do expectedTables+=$'taskcompass_dev\t'"$name"$'\t'"$right"$'\tNO\n'; done
            done
            expectedTables+=$'taskcompass_dev\tmigrationattempt\tSELECT\tNO\n'
         fi
      elif [[ "$account" == taskcompass_migrate && "$policy" == migration ]]; then
         for name in pillar project objective; do expectedTables+=$'taskcompass_dev\t'"$name"$'\tCREATE\tNO\n'; done
         for right in CREATE INSERT UPDATE; do expectedTables+=$'taskcompass_dev\tmigrationattempt\t'"$right"$'\tNO\n'; done
         for name in task commandrecord localchange; do expectedTables+=$'taskcompass_dev\t'"$name"$'\tALTER\tNO\n'; done
         expectedTables+=$'taskcompass_dev\tschemamigration\tINSERT\tNO\n'
      fi
      [[ "$(printf '%s' "${schemaRights[$account]:-}" | LC_ALL=C sort)" == "$(printf '%s' "$expectedSchema" | LC_ALL=C sort)" && "$(printf '%s' "${tableRights[$account]:-}" | LC_ALL=C sort)" == "$(printf '%s' "$expectedTables" | LC_ALL=C sort)" ]] || fail "Development grants differ for $account; do not broaden/repair."
   done
   if [[ "$platform" == windows ]]; then
      [[ -n "$publicGrants" ]] || fail 'Missing PUBLIC metadata.'
      local DATABASE_NAME=taskcompass_dev decodedIdentifier='' publicGrantCount=0 unrelatedGrantCount=0
      local -a grantRoleQueue=()
      local -A seenGrantRoles=()
      source "$repositoryDirectory/scripts/validate-grants.sh"
      parseGrantBatch PUBLIC "$publicGrants"
      [[ ${#grantRoleQueue[@]} == 0 ]] || fail 'PUBLIC inherited roles require review.'
   fi
   devMetadata=$metadata
   printf 'PASS: %s schema%s exact %s account grants; no unexpected authority/active sessions.\n' "$expectedHost" "$version" "$policy"
}
export75() {
   local sql=$1 destination=$2
   (set -o noclobber; query75 taskcompass_dev < "scripts/phase12/b7/$sql" > "$backupDirectory/$destination")
}
seal75() {
   local manifest=$1; shift
   (cd -- "$backupDirectory" && set -o noclobber && sha256sum "$@" > "$manifest")
   (cd -- "$backupDirectory" && sha256sum --check "$manifest")
}
checkHealth75() {
   local file=$1 line rule count extra expected='' actual=''
   local pattern="^SELECT '([^']+)' AS ruleName,COUNT"
   while IFS= read -r line; do
      if [[ "$line" == SELECT* ]]; then
         [[ "$line" =~ $pattern ]] || fail 'Unexpected health SQL format.'
         expected+="${BASH_REMATCH[1]}"$'\n'
      fi
   done < scripts/phase12/b7/schema2-health.sql
   while IFS=$'\t' read -r rule count extra; do
      count=${count%$'\r'}
      [[ -n "$rule" && "$count" == 0 && -z "$extra" ]] || fail 'Nonzero/malformed health result.'
      actual+="$rule"$'\n'
   done < "$backupDirectory/$file"
   [[ -n "$expected" && "$(printf '%s' "$expected" | LC_ALL=C sort)" == "$(printf '%s' "$actual" | LC_ALL=C sort)" ]] || fail 'Missing/extra CHECK/FK health rows.'
}
checkPost75() {
   local -a rows=()
   local index version checksum baselineFingerprint applied extra fingerprint prior started completed step state
   mapfile -t rows < "$backupDirectory/b75-upgrade-checks.tsv"
   for index in "${!rows[@]}"; do rows[$index]=${rows[$index]%$'\r'}; done
   [[ ${#rows[@]} == 9 && "${rows[0]}" == $'Pillar\t0' && "${rows[1]}" == $'Project\t0' && "${rows[2]}" == $'Objective\t0' ]] || fail 'New organization tables not empty.'
   for index in 3 4 5; do [[ "${rows[$index]}" == 0 ]] || fail 'Unexpected task owners/typed audit payloads.'; done
   IFS=$'\t' read -r version checksum baselineFingerprint applied extra <<< "${rows[6]}"
   [[ "$version" == 1 && "$checksum" == "$MIGRATION_1" && -z "$extra" ]] || fail 'Invalid original migration row.'
   IFS=$'\t' read -r version checksum fingerprint applied extra <<< "${rows[7]}"
   [[ "$version" == 2 && "$checksum" == "$MIGRATION_2" && -z "$extra" ]] || fail 'Invalid migration2 metadata.'
   IFS=$'\t' read -r version checksum prior started completed step state extra <<< "${rows[8]}"
   [[ "$version" == 2 && "$checksum" == "$MIGRATION_2" && "$prior" == "$baselineFingerprint" && "$started" =~ ^[1-9][0-9]*$ && "$completed" =~ ^[1-9][0-9]*$ && "$step" == 9 && "$state" == completed && -z "$extra" ]] || fail 'Incomplete/invalid migration journal.'
   (( completed >= started )) || fail 'Invalid migration journal times.'
}
full75() {
   local digest expected
   digest=$(query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql | sha256sum)
   expected=$(sha256sum < "$backupDirectory/b75-schema2.hex.tsv")
   [[ "${digest%% *}" == "${expected%% *}" ]] || fail 'Schema2 data/journal changed since migration preservation.'
}
# Definition-only diagnostic reuse; never replays a stage.
[[ "${TASKCOMPASS_B75_LIBRARY:-0}" != 1 ]] || return 0
if [[ "$b75Action" == preflight || "$b75Action" == grants ]]; then
   newFiles b75-grants-attempt.txt b75-grants-success.txt b75-grants.sha256 b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt
   baseline75 1
   adminStart; preflight75 original 1
   if [[ "$b75Action" == preflight ]]; then adminEnd; printf 'PASS: B7.5 fresh dev/schema1/backup/rehearsal/account baseline.\n'; exit 0; fi
   # No administrator prompt after mutations begin; repeat fresh data checks now.
   baseline75 1
   export75 legacy-export.sql b75-baseline.hex.tsv
   sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/b75-baseline.hex.tsv"
   (set -o noclobber; printf '%s' "$devMetadata" > "$backupDirectory/b75-before-grants.txt")
   writeMarker b75-grants-attempt.txt
   adminQuery scripts/phase12/b7/migration-dev-grants.sql
   [[ -z "$adminReport" ]] || fail 'Unexpected migration-grant output.'
   preflight75 migration 1
   (set -o noclobber; printf '%s' "$devMetadata" > "$backupDirectory/b75-after-grants.txt")
   baseline75 1; adminEnd
   writeMarker b75-grants-success.txt
   seal75 b75-grants.sha256 b75-grants-attempt.txt b75-grants-success.txt b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt
   printf 'PASS: exact temporary dev migrator grants; original data unchanged. Migration pending.\n'; exit 0
fi
checkManifest b75-grants.sha256 'b75-grants-attempt.txt b75-grants-success.txt b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt'
marker b75-grants-attempt.txt; marker b75-grants-success.txt
if [[ "$b75Action" == migrate ]]; then
   newFiles b75-migration-attempt.txt b75-migration-success.txt b75-preservation.sha256 b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv
   baseline75 1
   adminStart; preflight75 migration 1
   baseline75 1
   writeMarker b75-migration-attempt.txt
   output=$(TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306 TASKCOMPASS_DB_NAME=taskcompass_dev \
      TASKCOMPASS_DB_USER=taskcompass_migrate TASKCOMPASS_DB_ISOLATED=1 TASKCOMPASS_DB_PASSWORD="${passwords[taskcompass_migrate]}" \
      "$executable" --apply --to-version 2 2> >(redact)) || fail 'Migration failed/uncertain; preserve partial state.'
   output=${output//$'\r'/}; printf '%s\n' "$output"
   [[ "$output" == "Schema 2 verified; dataset $expectedDataset" ]] || fail 'Unexpected migration result.'
   verify75 2
   preflight75 migration 2; adminEnd
   baseline75 2
   export75 legacy-export.sql b75-legacy.hex.tsv
   export75 ddl2-export.sql b75-schema2.ddl.txt
   export75 upgrade-postconditions.sql b75-upgrade-checks.tsv
   export75 schema2-health.sql b75-health.tsv
   export75 schema2-export.sql b75-schema2.hex.tsv
   sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/b75-legacy.hex.tsv"
   bash "$b75Directory/compare-schema2-ddl.sh" "$backupDirectory/rehearsal2.ddl.txt" "$backupDirectory/b75-schema2.ddl.txt"
   checkPost75; checkHealth75 b75-health.tsv
   writeMarker b75-migration-success.txt
   seal75 b75-preservation.sha256 b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv
   printf 'PASS: dev schema2/UUID/journal verified; all original data/history preserved; runtime transition pending.\n'; exit 0
fi
checkManifest b75-preservation.sha256 'b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv'
marker b75-migration-attempt.txt; marker b75-migration-success.txt
checkPost75; checkHealth75 b75-health.tsv
if [[ "$b75Action" == runtime ]]; then
   newFiles b75-runtime-attempt.txt b75-runtime-success.txt b75-runtime.sha256 b75-final-grants.txt
   baseline75 2; full75
   # Check app login before changing grants, but do not require schema2 reads yet.
   authenticate taskcompass_app
   adminStart; preflight75 migration 2
   baseline75 2; full75
   writeMarker b75-runtime-attempt.txt
   adminQuery scripts/phase12/b7/runtime-dev-grants-and-revoke.sql
   [[ -z "$adminReport" ]] || fail 'Unexpected runtime grant result.'
   preflight75 final 2
   (set -o noclobber; printf '%s' "$devMetadata" > "$backupDirectory/b75-final-grants.txt")
   verify75 2 taskcompass_app
   baseline75 2; full75; adminEnd
   writeMarker b75-runtime-success.txt
   seal75 b75-runtime.sha256 b75-runtime-attempt.txt b75-runtime-success.txt b75-final-grants.txt
   printf 'PASS: final app19 rights; diagnostic/migrator SELECT-only; no data changes. Run stage05.\n'; exit 0
fi
checkManifest b75-runtime.sha256 'b75-runtime-attempt.txt b75-runtime-success.txt b75-final-grants.txt'
marker b75-runtime-attempt.txt; marker b75-runtime-success.txt
newFiles b75-verification-success.txt b75-final.sha256 b75-final-health.tsv b75-final.hex.tsv
baseline75 2; full75
adminStart; preflight75 final 2; adminEnd
verify75 2 taskcompass_app
# This preconnection identity guard uses a literal placeholder, never a real secret.
if guard=$(TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306 TASKCOMPASS_DB_NAME=taskcompass_dev \
   TASKCOMPASS_DB_USER=taskcompass_app TASKCOMPASS_DB_ISOLATED=1 TASKCOMPASS_DB_PASSWORD=runtime-guard-placeholder \
   "$executable" --apply --to-version 2 2>&1); then guardStatus=0; else guardStatus=$?; fi
[[ "$guardStatus" == 1 && "${guard//$'\r'/}" == 'Migration --apply requires the dedicated migration/test account, never runtime/admin credentials.' ]] || fail 'Runtime preconnection migration guard differs.'
export75 schema2-health.sql b75-final-health.tsv
checkHealth75 b75-final-health.tsv
export75 schema2-export.sql b75-final.hex.tsv
sameFiles "$backupDirectory/b75-schema2.hex.tsv" "$backupDirectory/b75-final.hex.tsv"
writeMarker b75-verification-success.txt
seal75 b75-final.sha256 b75-verification-success.txt b75-final-health.tsv b75-final.hex.tsv
printf 'PASS: B7.5 development upgrade/final privileges/preservation/health verified on %s.\nSTOP: no GUI acceptance, backup/restore, Windows authorization or Phase1.3 is implied.\n' "$platform"
