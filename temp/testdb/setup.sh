#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fresh isolated account/database provisioning only. No migrations or test execution.
set +x
set -euo pipefail
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
clientPath=''
adminUser=root
adminSocket=false
adminPassword=''
testPassword=''
confirmation=''
action=${1:---help}
[[ $# -eq 0 ]] || shift
trap 'unset adminPassword testPassword confirmation passwordSql setupSql MYSQL_PWD' EXIT

usage() {
   cat <<'USAGE'
Usage: bash temp/testdb/setup.sh --print-plan
       bash temp/testdb/setup.sh --apply [--admin-socket] [--admin-user NAME] [--client PATH]
No arguments: help only; no connection. --print-plan: placeholder SQL only.
--apply requires approved Stage 1 operations and an explicit confirmation.
Uses taskcompass_integration/taskcompass_test@127.0.0.1:3306 only.
Stops if either database or any same-name account exists; never repairs/resets it.
Prompts for passwords; writes no credentials or configuration files.
USAGE
}
fail() { printf '%s\n' "$1" >&2; exit 1; }
case "$action" in
   --help|-h) usage; exit 0 ;;
   --print-plan)
      [[ $# -eq 0 ]] || fail '--print-plan accepts no other arguments.'
      cat -- "$scriptDirectory/setup.template.sql"
      exit 0 ;;
   --apply) ;;
   *) usage; exit 2 ;;
esac
while [[ $# -gt 0 ]]; do
   case "$1" in
      --client|--admin-user)
         [[ $# -ge 2 ]] || fail "Missing value for $1"
         case "$1" in
            --client) clientPath=$2 ;;
            --admin-user) adminUser=$2 ;;
         esac
         shift 2 ;;
      --admin-socket) adminSocket=true; shift ;;
      *) fail "Unknown option: $1" ;;
   esac
done
if [[ -n "${MSYSTEM:-}" ]]; then
   [[ "$MSYSTEM" == UCRT64 ]] || fail 'Use the UCRT64 terminal on Windows.'
   ! "$adminSocket" || fail '--admin-socket is for Linux only.'
fi
if [[ -z "$clientPath" ]]; then
   clientPath=$(command -v mariadb || command -v mysql || true)
   if [[ -z "$clientPath" && "${MSYSTEM:-}" == UCRT64 ]]; then
      for candidate in '/c/Program Files'/MariaDB*/bin/mariadb.exe; do
         if [[ -f "$candidate" ]]; then
            [[ -z "$clientPath" ]] || fail 'Multiple MariaDB clients found; use --client.'
            clientPath=$candidate
         fi
      done
   fi
fi
[[ -n "$clientPath" && -x "$clientPath" ]] || fail 'MariaDB client not found; specify --client.'
# Read/validate the template before any connection or password is requested.
setupTemplate=$(cat -- "$scriptDirectory/setup.template.sql")
[[ "$setupTemplate" == *'{{TEST_PASSWORD_SQL_LITERAL}}'* ]] || fail 'Missing template password token.'
IFS= read -r -p 'Type CREATE taskcompass_integration to confirm the approved provisioning: ' confirmation || fail 'Cancelled.'
[[ "$confirmation" == 'CREATE taskcompass_integration' ]] || fail 'Confirmation mismatch; no connection made.'
unset confirmation
if ! "$adminSocket"; then
   IFS= read -r -s -p 'MariaDB administrator password: ' adminPassword || fail 'Cancelled.'
   printf '\n' >&2
fi

redactDiagnostics() {
   local line=''
   local escapedPassword=${testPassword//\'/\'\'}
   while IFS= read -r line || [[ -n "$line" ]]; do
      [[ -z "$escapedPassword" ]] || line=${line//"$escapedPassword"/<redacted>}
      [[ -z "$testPassword" ]] || line=${line//"$testPassword"/<redacted>}
      [[ -z "$adminPassword" ]] || line=${line//"$adminPassword"/<redacted>}
      printf '%s\n' "${line//$'\r'/}" >&2
   done
}
runAdmin() {
   local sql=$1
   local output=''
   local -a options=(--no-defaults --batch --raw --skip-column-names --connect-timeout=5 --default-character-set=utf8mb4 --user="$adminUser")
   if "$adminSocket"; then
      options+=(--protocol=SOCKET)
   else
      options+=(--protocol=TCP --host=127.0.0.1 --port=3306)
   fi
   if ! output=$(printf '%s\n' "$sql" | MYSQL_PWD="$adminPassword" "$clientPath" "${options[@]}" 2> >(redactDiagnostics)); then
      fail 'Administrator command failed. Check service/authentication; if provisioning began, preserve evidence and inspect before retrying.'
   fi
   printf '%s' "${output//$'\r'/}"
}
# Exact numeric output only; successful stderr warnings cannot become existence data.
existing=$(runAdmin "SELECT @@port,(SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='taskcompass_integration'),(SELECT COUNT(*) FROM mysql.user WHERE User='taskcompass_test');")
[[ "$existing" == $'3306\t0\t0' ]] || fail 'Database/account exists, server port differs from 3306, or inspection was inconclusive. Nothing changed; review it rather than rerun setup.'
IFS= read -r -s -p 'New integration password from your password manager (20+ characters): ' testPassword || fail 'Cancelled.'
printf '\n' >&2
[[ ${#testPassword} -ge 20 ]] || fail 'Use a distinct strong password of at least 20 characters.'
IFS= read -r -s -p 'Repeat new integration password: ' confirmation || fail 'Cancelled.'
printf '\n' >&2
[[ "$testPassword" == "$confirmation" ]] || fail 'Passwords differ; nothing created.'
unset confirmation
passwordSql="'${testPassword//\'/\'\'}'"
setupSql=${setupTemplate/'{{TEST_PASSWORD_SQL_LITERAL}}'/"$passwordSql"}
runAdmin "$setupSql" >/dev/null
unset testPassword passwordSql setupSql adminPassword
printf 'Created the isolated integration database/account with six scoped privileges.\nNo migrations/tests ran. Review grants and inherited policy before running tests.\n'
