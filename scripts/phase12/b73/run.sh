#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# B7.3 operator-only exact-snapshot clone. No reset, grants or migration apply.
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
readonly CREDENTIAL_DATABASE=taskcompass_phase12_test
source "$scriptDirectory/../read-only-login.sh"
trap 'unset accountPassword passwords TASKCOMPASS_PHASE12_TEST_DB_PASSWORD MYSQL_PWD' EXIT
fail() { printf 'STOP: %s Preserve target and artifacts; no retry/reset/repair.\n' "$1" >&2; exit 1; }
trap 'fail "B7.3 operation failed (exit $?)."' ERR
usage() {
   printf '%s\n' 'B7.3: 01-preflight.sh --approved; 02-import.sh --approved; 03-compare.sh --approved'
   printf '%s\n' 'Owner runs one stage at a time. Default/help/--print-plan never connects.'
   printf '%s\n' 'Optional --client PATH --build-dir PATH --backup-dir PATH (same pinned snapshot only).'
   printf '%s\n' 'Fixed empty taskcompass_phase12_test target only; no dev writes/reset/grants/migrations.'
}
[[ "$action" != help ]] || { usage; exit 0; }
case "$action" in preflight|import|compare) ;; *) fail 'Unknown stage.' ;; esac
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
"$approved" || fail 'Explicit B7.3 approval is required.'
case "$action" in preflight) stageNumber=01 ;; import) stageNumber=02 ;; compare) stageNumber=03 ;; esac
printf 'Stage %s/03: B7.3 %s\n' "$stageNumber" "$action"
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
   local response=''
   IFS= read -r -p "Type $1: " response || fail 'Cancelled.'
   [[ "$response" == "$1" ]] || fail 'Confirmation mismatch.'
}
preflight() {
   local report='' line='' tag='' values='' publicGrants='' schemaRights='' expectedRights='' name=''
   if [[ "$platform" == linux ]]; then
      env -u MYSQL_PWD sudo -v
      report=$(env -u MYSQL_PWD sudo -n -- "$clientPath" --no-defaults --protocol=SOCKET --user=root \
         --default-character-set=utf8mb4 --batch --raw --skip-column-names \
         --database=taskcompass_phase12_test < "$scriptDirectory/preflight.sql") || fail 'Root preflight failed before import.'
   else
      printf 'Enter Windows MariaDB ROOT password for read-only preflight.\n' >&2
      report=$(cat "$scriptDirectory/preflight.sql" scripts/phase12/b7/public-grants.sql |
         env -u MYSQL_PWD "$clientPath" --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
            --user=root --password --default-character-set=utf8mb4 --batch --raw --skip-column-names \
            --database=taskcompass_phase12_test) || fail 'Root preflight failed before import.'
   fi
   local -A seen=()
   while IFS= read -r line || [[ -n "$line" ]]; do
      line=${line%$'\r'}
      tag=${line%%$'\t'*}; values=${line#*$'\t'}
      case "$tag" in
         SERVER|EMPTY|HOST|GLOBAL|TABLE|COLUMN|ROLE)
            [[ ! -v "seen[$tag]" ]] || fail 'Duplicate preflight metadata.'
            seen[$tag]=$values ;;
         RIGHT) schemaRights+="$values"$'\n' ;;
         *)
            [[ "$platform" == windows && ( "$line" == GRANT\ * || "$line" == SET\ DEFAULT\ ROLE\ * ) ]] || fail 'Unexpected preflight output.'
            publicGrants+="$line"$'\n' ;;
      esac
   done <<< "$report"
   [[ "${seen[SERVER]:-}" == "$expectedHost"$'\t'"$expectedVersion"$'\t3306\ttaskcompass_phase12_test\troot@localhost\t'"$expectedTableCase"$'\t1\t1' ]] || fail 'Unexpected server/database/admin identity.'
   [[ "${seen[EMPTY]:-}" == $'1\t0\t0\t0\t0\t0' ]] || fail 'Target must be empty with correct encoding and no other sessions; no reset.'
   [[ "${seen[HOST]:-}" == 127.0.0.1 ]] || fail 'Unexpected test account host(s).'
   for tag in GLOBAL TABLE COLUMN ROLE; do [[ "${seen[$tag]:-}" == 0 ]] || fail 'Unexpected account privilege/role.'; done
   for name in SELECT INSERT UPDATE CREATE DROP ALTER; do expectedRights+=$'taskcompass\\_phase12\\_test\t'"$name"$'\tNO\n'; done
   [[ "$(printf '%s' "$schemaRights" | LC_ALL=C sort)" == "$(printf '%s' "$expectedRights" | LC_ALL=C sort)" ]] || fail 'Exact six scoped privileges required.'
   if [[ "$platform" == windows ]]; then
      [[ -n "$publicGrants" ]] || fail 'Missing PUBLIC report.'
      local DATABASE_NAME='' decodedIdentifier='' publicGrantCount=0 unrelatedGrantCount=0
      local -a grantRoleQueue=()
      local -A seenGrantRoles=()
      source "$repositoryDirectory/scripts/validate-grants.sh"
      for DATABASE_NAME in taskcompass_dev taskcompass_phase12_test; do parseGrantBatch PUBLIC "$publicGrants"; done
      [[ ${#grantRoleQueue[@]} == 0 ]] || fail 'PUBLIC roles require separate review.'
   fi
   printf 'PASS: %s / %s; empty fixed target, exact six grants, no roles/extras/PUBLIC writes.\n' "$expectedHost" "$expectedVersion"
}
marker() {
   local path=$1
   [[ -f "$path" && ! -L "$path" ]] || fail 'Missing/linked import marker.'
   [[ "$(cat "$path")" == "$expectedDigest" ]] || fail 'Import marker does not match the approved snapshot.'
}
snapshot
if [[ "$action" == preflight ]]; then preflight; exit 0; fi
if [[ "$action" == import ]]; then
   for name in b73-import-attempt.txt b73-import-success.txt rehearsal1.hex.tsv rehearsal1.ddl.txt b73-comparison.sha256; do
      [[ ! -e "$backupDirectory/$name" && ! -L "$backupDirectory/$name" ]] || fail 'Import/rehearsal evidence already exists; no reimport.'
   done
   printf 'Approved %s snapshot SHA-256: %s\n' "$platform" "$expectedDigest"
   confirm 'RESTORE taskcompass_phase12_test'
   confirm 'DUMP REVIEWED; WRITERS CLOSED'
   authenticate taskcompass_phase12_test
   preflight
   snapshot
   (set -o noclobber; printf '%s\n' "$expectedDigest" > "$backupDirectory/b73-import-attempt.txt")
   MYSQL_PWD="${passwords[taskcompass_phase12_test]}" "$clientPath" --no-defaults --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_test --default-character-set=utf8mb4 \
      --database=taskcompass_phase12_test < "$backupDirectory/after-gui.sql" 2> >(redact) || fail 'Import failed/uncertain; attempt marker retained, never retry/reset.'
   (set -o noclobber; printf '%s\n' "$expectedDigest" > "$backupDirectory/b73-import-success.txt")
   printf 'PASS: one-shot import exit 0. Next run 03-compare.sh; do not run integration/reset tests.\n'
   exit 0
fi
marker "$backupDirectory/b73-import-attempt.txt"
marker "$backupDirectory/b73-import-success.txt"
for name in rehearsal1.hex.tsv rehearsal1.ddl.txt b73-comparison.sha256; do
   [[ ! -e "$backupDirectory/$name" && ! -L "$backupDirectory/$name" ]] || fail 'Comparison evidence exists; do not overwrite/retry.'
done
authenticate taskcompass_phase12_test
output=$(TASKCOMPASS_PHASE12_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_PHASE12_TEST_DB_PORT=3306 \
   TASKCOMPASS_PHASE12_TEST_DB_NAME=taskcompass_phase12_test TASKCOMPASS_PHASE12_TEST_DB_USER=taskcompass_phase12_test \
   TASKCOMPASS_PHASE12_TEST_DB_ISOLATED=1 TASKCOMPASS_PHASE12_TEST_DB_PASSWORD="${passwords[taskcompass_phase12_test]}" \
   "$executable" --verify --phase12-test --to-version 1 2> >(redact)) || fail 'Restored schema verification failed.'
output=${output//$'\r'/}
printf '%s\n' "$output"
[[ "$output" == "Schema 1 verified; dataset $expectedDataset" ]] || fail 'Restored dataset/schema differs.'
exportClone() {
   local sql=$1 file=$2
   shift 2
   (set -o noclobber
    MYSQL_PWD="${passwords[taskcompass_phase12_test]}" "$clientPath" --no-defaults --protocol=TCP \
       --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_test \
       --default-character-set=utf8mb4 --batch --skip-column-names --database=taskcompass_phase12_test \
       "$@" < "$sql" > "$backupDirectory/$file" 2> >(redact))
}
exportClone scripts/phase12/b7/legacy-export.sql rehearsal1.hex.tsv
exportClone scripts/phase12/b7/ddl1-export.sql rehearsal1.ddl.txt --raw
sameFiles() {
   local first second
   first=$(sha256sum < "$1"); second=$(sha256sum < "$2")
   [[ "${first%% *}" == "${second%% *}" ]] || fail 'Restored data/DDL differs; preserve evidence.'
}
sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/rehearsal1.hex.tsv"
sameFiles "$backupDirectory/schema1.ddl.txt" "$backupDirectory/rehearsal1.ddl.txt"
(cd -- "$backupDirectory" && set -o noclobber && sha256sum rehearsal1.hex.tsv rehearsal1.ddl.txt \
   b73-import-attempt.txt b73-import-success.txt > b73-comparison.sha256)
(cd -- "$backupDirectory" && sha256sum --check b73-comparison.sha256)
printf 'PASS: B7.3 schema1/UUID/all legacy columns and history/counts/DDL match exact snapshot.\nPreserve populated clone; no reset, migration or grant changes authorized.\n'
