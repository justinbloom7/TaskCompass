#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fresh isolated account/database provisioning only. No migrations or test execution.
set +x
set -euo pipefail
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
purpose=${1:-}
[[ $# -eq 0 ]] || shift
case "$purpose" in
   phase11)
      databaseName=taskcompass_integration
      databaseUser=taskcompass_test
      templatePath="$scriptDirectory/../temp/testdb/setup.template.sql"
      entryPoint='temp/testdb/setup.sh' ;;
   phase12)
      databaseName=taskcompass_phase12_test
      databaseUser=taskcompass_phase12_test
      templatePath="$scriptDirectory/phase12/test-account.sql"
      entryPoint='scripts/phase12/03-setup-test.sh' ;;
   *) printf 'Choose the fixed phase11 or phase12 provisioning purpose.\n' >&2; exit 2 ;;
esac
readonly databaseName databaseUser templatePath entryPoint
clientPath=''
readonly MIN_PASSWORD_LENGTH=12
adminUser=root
adminSocket=false
adminSudo=false
sudoPath=''
adminPassword=''
testPassword=''
confirmation=''
action=${1:---help}
[[ $# -eq 0 ]] || shift
trap 'unset adminPassword testPassword confirmation passwordSql setupSql MYSQL_PWD' EXIT

usage() {
   local approvalOption=''
   [[ "$purpose" != phase12 ]] || approvalOption=' --approved'
   cat <<USAGE
Usage: bash $entryPoint --print-plan
       bash $entryPoint --apply$approvalOption [--admin-socket|--admin-sudo] [--admin-user NAME] [--client PATH]
No arguments: help only; no connection. --print-plan: placeholder SQL only.
--apply requires approved Stage 1 operations and an explicit confirmation.
Uses $databaseName/$databaseUser@127.0.0.1:3306 only.
Stops if either database or any same-name account exists; never repairs/resets it.
Default: TCP administrator password. --admin-socket: current OS user, no DB password.
Linux --admin-sudo: socket authentication with only the MariaDB client elevated;
sudo may ask for your OS password, then the script asks only for the NEW test password.
No automatic authentication fallback. No credentials or configuration files written.
USAGE
}
fail() { printf '%s\n' "$1" >&2; exit 1; }
case "$action" in
   --help|-h) usage; exit 0 ;;
   --print-plan)
      [[ $# -eq 0 ]] || fail '--print-plan accepts no other arguments.'
      cat -- "$templatePath"
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
      --admin-sudo) adminSudo=true; adminSocket=true; shift ;;
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
if "$adminSudo"; then
   [[ "$(uname -s)" == Linux ]] || fail '--admin-sudo is for Linux only.'
   sudoPath=$(command -v sudo || true)
   [[ -n "$sudoPath" && -x "$sudoPath" ]] || fail 'sudo not found; use --admin-socket from an already authorized OS account.'
fi
# Read/validate the template before any connection or password is requested.
setupTemplate=$(cat -- "$templatePath")
[[ "$setupTemplate" == *'{{TEST_PASSWORD_SQL_LITERAL}}'* ]] || fail 'Missing template password token.'
IFS= read -r -p "Type CREATE $databaseName to confirm the approved provisioning: " confirmation || fail 'Cancelled.'
[[ "$confirmation" == "CREATE $databaseName" ]] || fail 'Confirmation mismatch; no connection made.'
unset confirmation
if "$adminSudo"; then
   printf 'Using sudo for the MariaDB client over the local Unix socket; sudo may request your OS password.\n' >&2
   env -u MYSQL_PWD "$sudoPath" -v || fail 'sudo authorization failed; no database connection made.'
elif "$adminSocket"; then
   printf 'Using local Unix-socket administrator authentication as the current OS user; no MariaDB administrator password requested.\n' >&2
fi
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
   local -a command=("$clientPath" "${options[@]}")
   if "$adminSudo"; then
      # Authenticate sudo before SQL/secrets. -n fails closed if authorization expires.
      # Never use sudo -S: the client SQL stdin must not become a sudo password.
      command=(env -u MYSQL_PWD "$sudoPath" -n -- "${command[@]}")
   elif "$adminSocket"; then
      command=(env -u MYSQL_PWD "${command[@]}")
   fi
   if ! output=$(printf '%s\n' "$sql" | MYSQL_PWD="$adminPassword" "${command[@]}" 2> >(redactDiagnostics)); then
      fail 'Administrator command failed. Check service/authentication; if provisioning began, preserve evidence and inspect before retrying.'
   fi
   printf '%s' "${output//$'\r'/}"
}
if [[ "$purpose" == phase12 ]]; then
   serverIdentity=$(runAdmin 'SELECT @@hostname,VERSION(),@@port;')
   IFS=$'\t' read -r serverName serverVersion serverPort <<< "$serverIdentity"
   [[ "$serverPort" == 3306 && "$serverVersion" == *MariaDB* && "$serverVersion" =~ ^(10\.5\.|13\.) ]] || fail 'Phase 1.2 validation requires the approved MariaDB 10.5 or 13 server on port 3306.'
   printf 'Server: %s\nPlatform: %s / %s\n' "$serverIdentity" "$(uname -s)" "${MSYSTEM:-Linux}"
fi
# Exact numeric output only; successful stderr warnings cannot become existence data.
existing=$(runAdmin "SELECT @@port,(SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='$databaseName'),(SELECT COUNT(*) FROM mysql.user WHERE User='$databaseUser');")
[[ "$existing" == $'3306\t0\t0' ]] || fail 'Database/account exists, server port differs from 3306, or inspection was inconclusive. Nothing changed; review it rather than rerun setup.'
IFS= read -r -s -p "New integration password from your password manager (${MIN_PASSWORD_LENGTH}+ characters): " testPassword || fail 'Cancelled.'
printf '\n' >&2
[[ ${#testPassword} -ge "$MIN_PASSWORD_LENGTH" ]] || fail "Use a distinct strong password of at least $MIN_PASSWORD_LENGTH characters."
IFS= read -r -s -p 'Repeat new integration password: ' confirmation || fail 'Cancelled.'
printf '\n' >&2
[[ "$testPassword" == "$confirmation" ]] || fail 'Passwords differ; nothing created.'
unset confirmation
passwordSql="'${testPassword//\'/\'\'}'"
setupSql=${setupTemplate/'{{TEST_PASSWORD_SQL_LITERAL}}'/"$passwordSql"}
runAdmin "$setupSql" >/dev/null
unset testPassword passwordSql setupSql adminPassword
printf 'Created the isolated integration database/account with six scoped privileges.\nNo migrations/tests ran. Review grants and inherited policy before running tests.\n'
