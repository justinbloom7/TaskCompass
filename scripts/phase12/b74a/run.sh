#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# B7.4a operator-only narrow-role clone rehearsal. No dev writes or recovery.
set +x
set -Eeuo pipefail
umask 077
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$scriptDirectory/../../.." && pwd)
cd -- "$repositoryDirectory"
action=${1:-help}
[[ $# == 0 ]] || shift
approved=false
clientPath=''
buildDirectory=''
backupDirectory=''
accountPassword=''
# Definition-only callers may select the already approved development pair.
# Standalone rehearsal actions always retain their fixed clone identity.
if [[ "${TASKCOMPASS_B74_LIBRARY:-0}" == 1 && "${TASKCOMPASS_B74_CREDENTIAL_DATABASE:-}" == taskcompass_dev ]]; then
   readonly CREDENTIAL_DATABASE=taskcompass_dev
else
   readonly CREDENTIAL_DATABASE=taskcompass_phase12_test
fi
source "$scriptDirectory/../read-only-login.sh"
adminInput=''
adminOutput=''
adminPid=''
cleanup() {
   if [[ -n "$adminPid" ]]; then kill -TERM "$adminPid" 2>/dev/null || true; fi
   if [[ -n "$adminInput" ]]; then exec {adminInput}>&-; fi
   if [[ -n "$adminOutput" ]]; then exec {adminOutput}<&-; fi
   if [[ -n "$adminPid" ]]; then wait "$adminPid" 2>/dev/null || true; fi
   unset accountPassword passwords TASKCOMPASS_PHASE12_TEST_DB_PASSWORD MYSQL_PWD
}
trap cleanup EXIT
fail() { printf 'STOP: %s Preserve target and artifacts; no retry/reset/repair.\n' "$1" >&2; exit 1; }
trap 'fail "B7.4a operation failed (exit $?)."' ERR
usage() {
   printf '%s\n' 'B7.4a owner-run: 01-preflight.sh; 02-grants.sh; 03-migrate.sh; 04-preservation.sh (each --approved).'
   printf '%s\n' 'Help/--print-plan do nothing. Fixed populated phase12 test clone and pinned artifacts only.'
   printf '%s\n' 'Optional trusted --client PATH --build-dir PATH --backup-dir PATH; no target/hash override.'
   printf '%s\n' 'No reset/repair/resume, runtime-role swap, dev changes or GUI. Stop on any error.'
}
[[ "$action" != help ]] || { usage; exit 0; }
case "$action" in preflight|grants|migrate|preservation) ;; *) fail 'Unknown stage.' ;; esac
while [[ $# -gt 0 ]]; do
   case "$1" in
      --help|-h|--print-plan) usage; exit 0 ;;
      --approved) approved=true; shift ;;
      --client|--build-dir|--backup-dir)
         [[ $# -ge 2 && -n "$2" ]] || fail "Missing $1 value."
         case "$1" in --client) clientPath=$2 ;; --build-dir) buildDirectory=$2 ;; --backup-dir) backupDirectory=$2 ;; esac
         shift 2 ;;
      *) fail "Unknown option $1." ;;
   esac
done
"$approved" || fail 'Explicit B7.4a approval is required.'
case "$action" in preflight) stageNumber=01 ;; grants) stageNumber=02 ;; migrate) stageNumber=03 ;; preservation) stageNumber=04 ;; esac
[[ "${TASKCOMPASS_B74_LIBRARY:-0}" == 1 ]] || printf 'Stage %s/04: B7.4a %s\n' "$stageNumber" "$action"
if [[ -n "${MSYSTEM:-}" ]]; then
   [[ "$MSYSTEM" == UCRT64 ]] || fail 'Use MSYS2 UCRT64.'
   platform=windows
   clientPath=${clientPath:-'/c/Program Files/MariaDB 13.0/bin/mariadb.exe'}
   buildDirectory=${buildDirectory:-"$repositoryDirectory/build-ucrt64"}
   backupDirectory=${backupDirectory:-"$HOME/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917"}
   expectedTableCase=1
   expectedHost=feldria-w10
   expectedVersion=13.0.2-MariaDB
   expectedDataset=457bb52b-fea4-4347-9c47-675c6b035c90
   expectedDigest=6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}MYSQL_PWD;TASKCOMPASS_PHASE12_TEST_DB_PASSWORD"
else
   platform=linux
   clientPath=${clientPath:-/usr/bin/mariadb}
   buildDirectory=${buildDirectory:-"$repositoryDirectory/build"}
   backupDirectory=${backupDirectory:-"$HOME/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703"}
   expectedTableCase=0
   expectedHost=feledria
   expectedVersion=10.5.29-MariaDB
   expectedDataset=21be9da7-b767-4578-b972-cc9a35122ddb
   expectedDigest=83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26
fi
[[ -x "$clientPath" ]] || fail 'Native client missing.'
executable="$buildDirectory/TaskCompassMigrate"
[[ ! -f "$executable.exe" ]] || executable+=.exe
[[ -x "$executable" ]] || fail 'Built verifier missing.'
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
snapshot() {
   # Reuse private directory/manifest/path/UUID guards without starting a pager.
   bash "$scriptDirectory/../b72/04-review.sh" --backup-dir "$backupDirectory" --checks-only
   local actualVersion='' capturedVersion='' digest=''
   actualVersion=$(env -u MYSQL_PWD "$clientPath" --no-defaults --version)
   capturedVersion=$(cat "$backupDirectory/client-version.txt")
   [[ "${actualVersion//$'\r'/}" == "${capturedVersion//$'\r'/}" ]] || fail 'Native client version/path differs from sealed snapshot metadata.'
   digest=$(sha256sum < "$backupDirectory/after-gui.sql")
   [[ "${digest%% *}" == "$expectedDigest" ]] || fail 'Dump does not match the exact owner-approved platform hash.'
}
confirm() {
   # Owner requests no typed approval phrases; invocation supplies operator intent.
   # This notice consumes no stdin. Automatic identity/evidence guards still apply.
   printf 'Required precondition/operation: %s\n' "$1"
}
readonly MIGRATION_1=bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0
readonly MIGRATION_2=7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736
readonly GRANT_HASH=e86abce5e76d23639a665e2bca51cd1d87bc335451db50122ed505903470e9df
receipt="$expectedDigest $MIGRATION_2 $GRANT_HASH"
privateFile() {
   [[ -f "$backupDirectory/$1" && ! -L "$backupDirectory/$1" ]] || fail 'Missing/linked private evidence.'
}
newFiles() {
   local name
   for name in "$@"; do [[ ! -e "$backupDirectory/$name" && ! -L "$backupDirectory/$name" ]] || fail 'Prior/partial stage evidence exists; review required.'; done
}
marker() {
   privateFile "$1"
   [[ "$(cat "$backupDirectory/$1")" == "$receipt" ]] || fail 'Wrong stage receipt.'
}
writeMarker() { (set -o noclobber; printf '%s\n' "$receipt" > "$backupDirectory/$1"); }
sameFiles() {
   local left right
   left=$(sha256sum < "$1"); right=$(sha256sum < "$2")
   [[ "${left%% *}" == "${right%% *}" ]] || fail 'Legacy data/DDL drift.'
}
# Guard known receipt paths before invoking checksum utility.
checkRehearsalEvidence() {
   local line name names='' expected='rehearsal1.hex.tsv rehearsal1.ddl.txt b73-import-attempt.txt b73-import-success.txt'
   privateFile b73-comparison.sha256
   while IFS= read -r line || [[ -n "$line" ]]; do
      line=${line%$'\r'}
      [[ "$line" =~ ^[0-9a-fA-F]{64}\ [\ \*](.+)$ ]] || fail 'Malformed rehearsal checksum.'
      name=${BASH_REMATCH[1]}
      [[ "$name" != *[![:alnum:]._-]* && " $expected " == *" $name "* && " $names " != *" $name "* ]] || fail 'Unexpected/duplicate rehearsal checksum path.'
      privateFile "$name"
      names+="$name "
   done < "$backupDirectory/b73-comparison.sha256"
   [[ "$(printf '%s\n' $names | LC_ALL=C sort)" == "$(printf '%s\n' $expected | LC_ALL=C sort)" ]] || fail 'Incomplete rehearsal checksums.'
   (cd -- "$backupDirectory" && sha256sum --check b73-comparison.sha256)
   for name in b73-import-attempt.txt b73-import-success.txt; do
      [[ "$(cat "$backupDirectory/$name")" == "$expectedDigest" ]] || fail 'Wrong imported snapshot receipt.'
   done
   sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/rehearsal1.hex.tsv"
   sameFiles "$backupDirectory/schema1.ddl.txt" "$backupDirectory/rehearsal1.ddl.txt"
}
checkSql() {
   local version expected digest sourceFile
   for version in 1 2; do
      expected=$MIGRATION_1; [[ "$version" != 2 ]] || expected=$MIGRATION_2
      sourceFile=migrations/mariadb/001_PersistentTaskFoundation.sql
      [[ "$version" != 2 ]] || sourceFile=migrations/mariadb/002_OrganizationalManagement.sql
      digest=$(sha256sum < "$sourceFile")
      [[ "${digest%% *}" == "$expected" ]] || fail 'Migration source changed.'
      digest=$("$executable" --print-migration "$version" | tr -d '\r' | sha256sum)
      [[ "${digest%% *}" == "$expected" ]] || fail 'Embedded migration differs; rebuild/review before any mutation.'
   done
   digest=$(sha256sum < scripts/phase12/b7/rehearsal-migration-grants.sql)
   [[ "${digest%% *}" == "$GRANT_HASH" ]] || fail 'Grant SQL differs from approval.'
}
query() {
   MYSQL_PWD="${passwords[taskcompass_phase12_test]}" "$clientPath" --no-defaults --skip-reconnect --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_test --default-character-set=utf8mb4 \
      --database=taskcompass_phase12_test --batch --raw --skip-column-names "$@" 2> >(redact)
}
runVerify() {
   local version=$1 output
   authenticate taskcompass_phase12_test
   output=$(TASKCOMPASS_PHASE12_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_PHASE12_TEST_DB_PORT=3306 \
      TASKCOMPASS_PHASE12_TEST_DB_NAME=taskcompass_phase12_test TASKCOMPASS_PHASE12_TEST_DB_USER=taskcompass_phase12_test \
      TASKCOMPASS_PHASE12_TEST_DB_ISOLATED=1 TASKCOMPASS_PHASE12_TEST_DB_PASSWORD="${passwords[taskcompass_phase12_test]}" \
      "$executable" --verify --phase12-test --to-version "$version" 2> >(redact)) || fail 'Fresh schema verification failed.'
   output=${output//$'\r'/}; printf '%s\n' "$output"
   [[ "$output" == "Schema $version verified; dataset $expectedDataset" ]] || fail 'Unexpected schema/dataset.'
}
baseline() {
   runVerify 1
   local current expected
   current=$(query < scripts/phase12/b7/legacy-export.sql | sha256sum)
   expected=$(sha256sum < "$backupDirectory/legacy-before.hex.tsv")
   [[ "${current%% *}" == "${expected%% *}" ]] || fail 'Original values/history/metadata changed before mutation.'
   current=$(query < scripts/phase12/b7/ddl1-export.sql | sha256sum)
   expected=$(sha256sum < "$backupDirectory/schema1.ddl.txt")
   [[ "${current%% *}" == "${expected%% *}" ]] || fail 'Schema1 DDL drift.'
   printf 'PASS: fresh schema1/UUID/fingerprint/all original values and raw DDL match sealed baseline.\n'
}
# One native admin session per stage: password stays in native client, not script.
# Explicit flush and no reconnect; only approved metadata SELECTs + exact grant file.
adminStart() {
   local -a command=("$clientPath" --no-defaults --skip-reconnect --skip-auto-rehash --unbuffered --batch --raw --skip-column-names
                    --default-character-set=utf8mb4 --connect-timeout=5 --user=root --database="$CREDENTIAL_DATABASE")
   if [[ "$platform" == linux ]]; then
      env -u MYSQL_PWD sudo -v
      command=(env -u MYSQL_PWD sudo -n -- "${command[@]}" --protocol=SOCKET)
   else
      printf 'Enter Windows MariaDB ROOT password for this native administrator session.\n' >&2
      command=(env -u MYSQL_PWD "${command[@]}" --protocol=TCP --host=127.0.0.1 --port=3306 --password)
   fi
   coproc B74_ADMIN { exec "${command[@]}"; }
   adminPid=$B74_ADMIN_PID
   exec {adminInput}>&"${B74_ADMIN[1]}"
   exec {adminOutput}<&"${B74_ADMIN[0]}"
   # Close the coprocess originals too; otherwise stdin stays open at adminEnd.
   local originalInput=${B74_ADMIN[1]} originalOutput=${B74_ADMIN[0]}
   exec {originalInput}>&-
   exec {originalOutput}<&-
}
adminQuery() {
   local line
   adminReport=''
   printf '%s\nSELECT '\''__TASKCOMPASS_B74_END__'\'';\n' "$(cat "$1")" >&"$adminInput" || fail 'Administrator session unavailable.'
   while IFS= read -r line <&"$adminOutput"; do
      line=${line%$'\r'}
      [[ "$line" != __TASKCOMPASS_B74_END__ ]] || return 0
      adminReport+="$line"$'\n'
   done
   fail 'Administrator command failed/connection lost; preserve any attempted transition.'
}
adminEnd() {
   exec {adminInput}>&-; adminInput=''
   wait "$adminPid" || fail 'Uncertain administrator session outcome.'
   adminPid=''
   exec {adminOutput}<&-; adminOutput=''
}
preflight() {
   local policy=$1 version=$2 line tag values publicGrants='' rights='' tableRights='' objects='' expected='' expectedTables='' name
   local -A seen=()
   adminQuery "$scriptDirectory/preflight.sql"
   if [[ "$platform" == windows ]]; then
      local metadata=$adminReport
      adminQuery scripts/phase12/b7/public-grants.sql
      publicGrants=$adminReport
      adminReport=$metadata
   fi
   while IFS= read -r line; do
      [[ -n "$line" ]] || continue
      tag=${line%%$'\t'*}; values=${line#*$'\t'}
      case "$tag" in
         SERVER|STATE|HOST|GLOBAL|COLUMN|ROLE)
            [[ ! -v "seen[$tag]" ]] || fail 'Duplicate metadata guard.'
            seen[$tag]=$values ;;
         RIGHT) rights+="$values"$'\n' ;;
         TABLE_RIGHT) tableRights+="$values"$'\n' ;;
         OBJECT) objects+="$values"$'\n' ;;
         *) fail 'Unexpected metadata output.' ;;
      esac
   done <<< "$adminReport"
   [[ "${seen[SERVER]:-}" == "$expectedHost"$'\t'"$expectedVersion"$'\t3306\ttaskcompass_phase12_test\troot@localhost\t'"$expectedTableCase"$'\t1\t1' ]] || fail 'Wrong server/database/admin/session identity.'
   [[ "${seen[STATE]:-}" == $'1\t0\t0\t0\t0' && "${seen[HOST]:-}" == 127.0.0.1 ]] || fail 'Encoding/stored-code/session/account-host mismatch.'
   for tag in GLOBAL COLUMN ROLE; do [[ "${seen[$tag]:-}" == 0 ]] || fail 'Unexpected global/column/role authority.'; done
   for name in commandrecord dataset localchange schemamigration task; do expectedTables+="$name"$'\tBASE TABLE\tInnoDB\n'; done
   if [[ "$version" == 2 ]]; then
      for name in migrationattempt pillar project objective; do expectedTables+="$name"$'\tBASE TABLE\tInnoDB\n'; done
   fi
   [[ "$(printf '%s' "$objects" | LC_ALL=C sort)" == "$(printf '%s' "$expectedTables" | LC_ALL=C sort)" ]] || fail 'Unexpected populated schema objects/engine.'
   case "$policy" in
      broad)
         for name in SELECT INSERT UPDATE CREATE DROP ALTER; do expected+=$'taskcompass\\_phase12\\_test\t'"$name"$'\tNO\n'; done
         [[ -z "$tableRights" ]] || fail 'Unexpected table grants before transition.' ;;
      narrow)
         expected=$'taskcompass\\_phase12\\_test\tSELECT\tNO\n'
         local expectedTableRights=''
         for name in pillar project objective; do expectedTableRights+=$'taskcompass_phase12_test\t'"$name"$'\tCREATE\tNO\n'; done
         for name in CREATE INSERT UPDATE; do expectedTableRights+=$'taskcompass_phase12_test\tmigrationattempt\t'"$name"$'\tNO\n'; done
         for name in task commandrecord localchange; do expectedTableRights+=$'taskcompass_phase12_test\t'"$name"$'\tALTER\tNO\n'; done
         expectedTableRights+=$'taskcompass_phase12_test\tschemamigration\tINSERT\tNO\n'
         [[ "$(printf '%s' "$tableRights" | LC_ALL=C sort)" == "$(printf '%s' "$expectedTableRights" | LC_ALL=C sort)" ]] || fail 'Narrow table grants differ; do not broaden.' ;;
      runtime)
         local expectedTableRights=''
         for name in task pillar project objective; do
            for right in SELECT INSERT UPDATE; do expectedTableRights+=$'taskcompass_phase12_test\t'"$name"$'\t'"$right"$'\tNO\n'; done
         done
         for name in commandrecord localchange; do
            for right in SELECT INSERT; do expectedTableRights+=$'taskcompass_phase12_test\t'"$name"$'\t'"$right"$'\tNO\n'; done
         done
         for name in dataset schemamigration migrationattempt; do expectedTableRights+=$'taskcompass_phase12_test\t'"$name"$'\tSELECT\tNO\n'; done
         [[ "$(printf '%s' "$tableRights" | LC_ALL=C sort)" == "$(printf '%s' "$expectedTableRights" | LC_ALL=C sort)" ]] || fail 'Runtime table grants differ.' ;;
      *) fail 'Unknown role policy.' ;;
   esac
   [[ "$(printf '%s' "$rights" | LC_ALL=C sort)" == "$(printf '%s' "$expected" | LC_ALL=C sort)" ]] || fail 'Schema grants differ from approved state.'
   if [[ "$platform" == windows ]]; then
      [[ -n "$publicGrants" ]] || fail 'Missing PUBLIC metadata.'
      local DATABASE_NAME='' decodedIdentifier='' publicGrantCount=0 unrelatedGrantCount=0
      local -a grantRoleQueue=()
      local -A seenGrantRoles=()
      source "$repositoryDirectory/scripts/validate-grants.sh"
      for DATABASE_NAME in taskcompass_dev taskcompass_phase12_test; do parseGrantBatch PUBLIC "$publicGrants"; done
      [[ ${#grantRoleQueue[@]} == 0 ]] || fail 'PUBLIC roles require review.'
   fi
   printf 'PASS: fresh %s schema%s on %s/%s; exact %s grants and no unexpected authority/sessions.\n' "$CREDENTIAL_DATABASE" "$version" "$expectedHost" "$expectedVersion" "$policy"
}
# Definition-only reuse by B7.4b; this path performs no database operation.
[[ "${TASKCOMPASS_B74_LIBRARY:-0}" != 1 ]] || return 0
snapshot
checkRehearsalEvidence
checkSql
if [[ "$action" == preflight ]]; then
   newFiles b74a-grants-attempt.txt b74a-grants-success.txt b74a-migration-attempt.txt b74a-migration-success.txt
   baseline
   adminStart; preflight broad 1; adminEnd
   exit 0
fi
if [[ "$action" == grants ]]; then
   newFiles b74a-grants-attempt.txt b74a-grants-success.txt b74a-migration-attempt.txt b74a-migration-success.txt
   confirm 'NARROW GRANTS taskcompass_phase12_test'
   confirm 'WRITERS CLOSED; BASELINE UNCHANGED'
   baseline
   adminStart; preflight broad 1
   # Repeat original-data check after administrator authentication, before mutation.
   baseline
   writeMarker b74a-grants-attempt.txt
   adminQuery scripts/phase12/b7/rehearsal-migration-grants.sql
   [[ -z "$adminReport" ]] || fail 'Unexpected grant output; inspect partial grants.'
   preflight narrow 1
   baseline
   adminEnd
   writeMarker b74a-grants-success.txt
   printf 'PASS: exact narrow grant transition verified; original data unchanged. No migration yet.\n'
   exit 0
fi
marker b74a-grants-attempt.txt; marker b74a-grants-success.txt
if [[ "$action" == migrate ]]; then
   newFiles b74a-migration-attempt.txt b74a-migration-success.txt
   confirm 'MIGRATE REHEARSAL taskcompass_phase12_test TO 2'
   confirm 'WRITERS CLOSED; BASELINE UNCHANGED'
   baseline
   adminStart; preflight narrow 1; adminEnd
   baseline
   writeMarker b74a-migration-attempt.txt
   output=$(TASKCOMPASS_PHASE12_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_PHASE12_TEST_DB_PORT=3306 \
      TASKCOMPASS_PHASE12_TEST_DB_NAME=taskcompass_phase12_test TASKCOMPASS_PHASE12_TEST_DB_USER=taskcompass_phase12_test \
      TASKCOMPASS_PHASE12_TEST_DB_ISOLATED=1 TASKCOMPASS_PHASE12_TEST_DB_PASSWORD="${passwords[taskcompass_phase12_test]}" \
      "$executable" --apply --phase12-test --to-version 2 2> >(redact)) || fail 'Migration failed/uncertain; no resume/retry.'
   output=${output//$'\r'/}; printf '%s\n' "$output"
   [[ "$output" == "Schema 2 verified; dataset $expectedDataset" ]] || fail 'Unexpected apply result.'
   runVerify 2
   writeMarker b74a-migration-success.txt
   printf 'PASS: one-shot clone migration002 and schema2 verification; preservation stage still required.\n'
   exit 0
fi
marker b74a-migration-attempt.txt; marker b74a-migration-success.txt
newFiles rehearsal2-legacy.hex.tsv rehearsal2.ddl.txt b74a-upgrade-checks.tsv b74a-health.tsv b74a-preservation.sha256
runVerify 2
adminStart; preflight narrow 2; adminEnd
(set -o noclobber
 query < scripts/phase12/b7/legacy-export.sql > "$backupDirectory/rehearsal2-legacy.hex.tsv"
 query < scripts/phase12/b7/ddl2-export.sql > "$backupDirectory/rehearsal2.ddl.txt"
 query < scripts/phase12/b7/upgrade-postconditions.sql > "$backupDirectory/b74a-upgrade-checks.tsv"
 query < scripts/phase12/b7/schema2-health.sql > "$backupDirectory/b74a-health.tsv")
sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/rehearsal2-legacy.hex.tsv"
mapfile -t post < "$backupDirectory/b74a-upgrade-checks.tsv"
for index in "${!post[@]}"; do post[$index]=${post[$index]%$'\r'}; done
[[ ${#post[@]} == 9 && "${post[0]}" == $'Pillar\t0' && "${post[1]}" == $'Project\t0' && "${post[2]}" == $'Objective\t0' ]] || fail 'New organization tables are not empty.'
for index in 3 4 5; do [[ "${post[$index]}" == 0 ]] || fail 'Unexpected legacy ownership/typed targets/payload version.'; done
IFS=$'\t' read -r version checksum baselineFingerprint applied extra <<< "${post[6]}"
[[ "$version" == 1 && "$checksum" == "$MIGRATION_1" && -z "$extra" ]] || fail 'Bad migration1 metadata.'
IFS=$'\t' read -r version checksum fingerprint applied extra <<< "${post[7]}"
[[ "$version" == 2 && "$checksum" == "$MIGRATION_2" && -z "$extra" ]] || fail 'Bad migration2 metadata.'
IFS=$'\t' read -r version checksum prior started completed step state extra <<< "${post[8]}"
[[ "$version" == 2 && "$checksum" == "$MIGRATION_2" && "$prior" == "$baselineFingerprint" && "$started" =~ ^[1-9][0-9]*$ && "$completed" =~ ^[1-9][0-9]*$ && "$step" == 9 && "$state" == completed && -z "$extra" ]] || fail 'Bad attempt journal.'
(( completed >= started )) || fail 'Invalid attempt timestamps.'
expectedRules=''; actualRules=''
pattern="^SELECT '([^']+)' AS ruleName,COUNT"
while IFS= read -r line; do
   if [[ "$line" == SELECT* ]]; then
      [[ "$line" =~ $pattern ]] || fail 'Unsupported health query format.'
      expectedRules+="${BASH_REMATCH[1]}"$'\n'
   fi
done < scripts/phase12/b7/schema2-health.sql
while IFS=$'\t' read -r name violations extra; do
   violations=${violations%$'\r'}
   [[ -n "$name" && "$violations" == 0 && -z "$extra" ]] || fail 'CHECK/FK violation or malformed health result.'
   actualRules+="$name"$'\n'
done < "$backupDirectory/b74a-health.tsv"
[[ -n "$expectedRules" && "$(printf '%s' "$expectedRules" | LC_ALL=C sort)" == "$(printf '%s' "$actualRules" | LC_ALL=C sort)" ]] || fail 'Missing/extra health rule results.'
(cd -- "$backupDirectory" && set -o noclobber && sha256sum rehearsal2-legacy.hex.tsv rehearsal2.ddl.txt \
   b74a-upgrade-checks.tsv b74a-health.tsv b74a-grants-attempt.txt b74a-grants-success.txt \
   b74a-migration-attempt.txt b74a-migration-success.txt > b74a-preservation.sha256)
(cd -- "$backupDirectory" && sha256sum --check b74a-preservation.sha256)
printf 'PASS: B7.4a narrow-role schema2 clone; all original data/history/metadata preserved, new owners empty and every CHECK/FK health rule zero.\nSTOP: runtime role/probe and development migration remain unauthorized.\n'
