#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only admin preflight before explicit run-owned destructive integration tests.
set +x
set -euo pipefail
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$scriptDirectory/../.." && pwd)
approved=false
clientPath=''
buildDirectory=''
adminPassword=''
adminSocket=false
adminSudo=false
sudoPath=''
accountPassword=''
confirmation=''
trap 'unset adminPassword accountPassword confirmation TASKCOMPASS_PHASE12_TEST_DB_PASSWORD MYSQL_PWD' EXIT
fail() { printf 'STOP: %s\n' "$1" >&2; exit 1; }
usage() {
   printf 'Usage: bash scripts/phase12/04-integration-test.sh --approved [--client PATH] [--build-dir PATH] [--admin-socket|--admin-sudo]\nDefault/help/--print-plan: no connection. Obtain separate approval for live tests.\n'
}
[[ $# != 0 ]] || { usage; exit 0; }
while [[ $# -gt 0 ]]; do
   case "$1" in
      --help|-h) usage; exit 0 ;;
      --print-plan) exec bash "$scriptDirectory/02-review-plan.sh" ;;
      --approved) approved=true; shift ;;
      --admin-socket) adminSocket=true; shift ;;
      --admin-sudo) adminSocket=true; adminSudo=true; shift ;;
      --client|--build-dir)
         [[ $# -ge 2 && -n "$2" ]] || fail "Missing value for $1"
         case "$1" in --client) clientPath=$2 ;; --build-dir) buildDirectory=$2 ;; esac
         shift 2 ;;
      *) fail "Unknown option: $1" ;;
   esac
done
"$approved" || fail 'Explicit live-test approval required; no connection made.'
if [[ -n "${MSYSTEM:-}" ]]; then
   [[ "$MSYSTEM" == UCRT64 ]] || fail 'Use MSYS2 UCRT64.'
   ! "$adminSocket" || fail 'Socket administrator options are for Linux only.'
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}MYSQL_PWD;TASKCOMPASS_PHASE12_TEST_DB_PASSWORD"
fi
if [[ -z "$buildDirectory" ]]; then
   buildDirectory="$repositoryDirectory/build"
   [[ "${MSYSTEM:-}" != UCRT64 ]] || buildDirectory="$repositoryDirectory/build-ucrt64"
fi
executable="$buildDirectory/OrganizationIntegrationTests"
[[ ! -f "$executable.exe" ]] || executable+=.exe
[[ -x "$executable" ]] || fail 'Build OrganizationIntegrationTests first.'
for setting in HOST PORT NAME USER ISOLATED; do
   variableName="TASKCOMPASS_PHASE12_TEST_DB_$setting"
   case "$setting" in HOST) expected=127.0.0.1 ;; PORT) expected=3306 ;; NAME|USER) expected=taskcompass_phase12_test ;; ISOLATED) expected=1 ;; esac
   [[ ! -v "$variableName" || "${!variableName}" == "$expected" ]] || fail "Conflicting $variableName; no connection made."
   export "$variableName=$expected"
done
if [[ -z "$clientPath" ]]; then
   clientPath=$(command -v mariadb || command -v mysql || true)
   if [[ -z "$clientPath" && "${MSYSTEM:-}" == UCRT64 ]]; then
      for candidate in '/c/Program Files'/MariaDB*/bin/mariadb.exe; do
         [[ ! -f "$candidate" ]] || { [[ -z "$clientPath" ]] || fail 'Multiple clients; use --client.'; clientPath=$candidate; }
      done
   fi
fi
[[ -x "$clientPath" ]] || fail 'Specify an executable MariaDB --client.'
if "$adminSudo"; then
   [[ "$(uname -s)" == Linux ]] || fail '--admin-sudo is for Linux only.'
   sudoPath=$(command -v sudo || true)
   [[ -x "$sudoPath" ]] || fail 'sudo not found; use --admin-socket from an authorized OS account.'
fi
IFS= read -r -p 'Type TEST taskcompass_phase12_test WITH RUN-OWNED RESET: ' confirmation || fail 'Cancelled.'
[[ "$confirmation" == 'TEST taskcompass_phase12_test WITH RUN-OWNED RESET' ]] || fail 'Confirmation mismatch; no connection made.'
unset confirmation
if "$adminSudo"; then
   printf 'Using sudo for the read-only administrator client over the Unix socket; sudo may request your OS password.\n' >&2
   env -u MYSQL_PWD "$sudoPath" -v || fail 'sudo authorization failed; no database connection made.'
elif ! "$adminSocket"; then
   IFS= read -r -s -p 'MariaDB administrator password (read-only preflight): ' adminPassword || fail 'Cancelled.'
   printf '\n' >&2
fi
redact() {
   local line=''
   while IFS= read -r line || [[ -n "$line" ]]; do
      [[ -z "$adminPassword" ]] || line=${line//"$adminPassword"/<redacted>}
      printf '%s\n' "${line//$'\r'/}" >&2
   done
}
adminQuery() {
   local output=''
   local -a command=("$clientPath" --no-defaults --user=root --batch --raw --skip-column-names --connect-timeout=5 --default-character-set=utf8mb4)
   if "$adminSocket"; then
      command+=(--protocol=SOCKET)
      if "$adminSudo"; then
         command=(env -u MYSQL_PWD "$sudoPath" -n -- "${command[@]}")
      else
         command=(env -u MYSQL_PWD "${command[@]}")
      fi
   else
      command+=(--protocol=TCP --host=127.0.0.1 --port=3306)
   fi
   output=$(printf '%s\n' "$1" | MYSQL_PWD="$adminPassword" "${command[@]}" 2> >(redact)) || fail 'Read-only preflight failed; no tests ran.'
   printf '%s' "${output//$'\r'/}"
}
server=$(adminQuery 'SELECT @@hostname,VERSION(),@@port;')
[[ "$server" == *$'\t3306' ]] || fail 'Unexpected server port.'
printf 'Server: %s\nPlatform: %s / %s\n' "$server" "$(uname -s)" "${MSYSTEM:-Linux}"
state=$(adminQuery "SELECT (SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_phase12_test' AND DEFAULT_CHARACTER_SET_NAME='utf8mb4' AND DEFAULT_COLLATION_NAME='utf8mb4_unicode_ci'),(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='taskcompass_phase12_test'),(SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='taskcompass_phase12_test'),(SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='taskcompass_phase12_test'),(SELECT COUNT(*) FROM information_schema.EVENTS WHERE EVENT_SCHEMA='taskcompass_phase12_test');")
[[ "$state" == $'1\t0\t0\t0\t0' ]] || fail 'Target must be entirely empty with approved encoding. No reset attempted.'
[[ "$(adminQuery "SELECT Host FROM mysql.user WHERE User='taskcompass_phase12_test';")" == 127.0.0.1 ]] || fail 'Unexpected account host(s).'
grantee="CONCAT(CHAR(39),'taskcompass_phase12_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39))"
for sql in \
   "SELECT COUNT(*) FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=$grantee AND (PRIVILEGE_TYPE<>'USAGE' OR IS_GRANTABLE<>'NO');" \
   "SELECT COUNT(*) FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=$grantee;" \
   "SELECT COUNT(*) FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE=$grantee;" \
   "SELECT COUNT(*) FROM mysql.roles_mapping WHERE User='taskcompass_phase12_test';"; do
   [[ "$(adminQuery "$sql")" == 0 ]] || fail 'Unexpected global/table/column privilege or role; tests not run.'
done
actual=$(adminQuery "SELECT CONCAT(TABLE_SCHEMA,'|',PRIVILEGE_TYPE,'|',IS_GRANTABLE) FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=$grantee;")
expected=''
for privilege in SELECT INSERT UPDATE CREATE DROP ALTER; do expected+="taskcompass\\_phase12\\_test|$privilege|NO"$'\n'; done
[[ "$(printf '%s\n' "$actual" | LC_ALL=C sort)" == "$(printf '%s' "$expected" | LC_ALL=C sort)" ]] || fail 'Direct grants differ from exact six scoped privileges.'
version=$(adminQuery 'SELECT VERSION();')
[[ "$version" =~ ^([0-9]+)\.([0-9]+) ]] || fail 'Unknown server version.'
if (( BASH_REMATCH[1] > 10 || (BASH_REMATCH[1] == 10 && BASH_REMATCH[2] >= 11) )); then
   source "$scriptDirectory/../validate-grants.sh"
   DATABASE_NAME=taskcompass_phase12_test
   publicGrantCount=0 unrelatedGrantCount=0 decodedIdentifier=''
   declare -a grantRoleQueue=()
   declare -A seenGrantRoles=()
   publicGrants=$(adminQuery 'SHOW GRANTS FOR PUBLIC;')
   [[ -n "$publicGrants" ]] || fail 'Cannot establish PUBLIC policy.'
   parseGrantBatch PUBLIC "$publicGrants"
   [[ ${#grantRoleQueue[@]} == 0 ]] || fail 'PUBLIC inherited roles require review.'
fi
printf 'PASS: empty fixed target, exact direct grants, no explicit roles, PUBLIC reviewed.\n'
unset adminPassword
IFS= read -r -s -p 'Password for taskcompass_phase12_test: ' accountPassword || fail 'Cancelled.'
printf '\n' >&2
[[ -n "$accountPassword" ]] || fail 'Empty test password.'
export TASKCOMPASS_PHASE12_TEST_DB_PASSWORD="$accountPassword"
unset accountPassword
set +e
"$executable" --run --allow-reset
result=$?
set -e
unset TASKCOMPASS_PHASE12_TEST_DB_PASSWORD
printf 'Organization integration test exit code: %s\n' "$result"
exit "$result"
