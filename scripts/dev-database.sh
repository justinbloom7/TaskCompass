#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated development tooling only. No application tables or migrations.
set +x
set -euo pipefail

readonly DATABASE_NAME=taskcompass_dev
readonly DATABASE_USER=taskcompass_dev
readonly DATABASE_HOST=127.0.0.1
readonly DATABASE_PORT=3306
readonly GRANTEE_SQL="CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39))"
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$scriptDirectory/.." && pwd)
action=${1:-help}
[[ $# -eq 0 ]] || shift
clientPath=''
buildDirectory=''
adminSocket=false
adminUser=root
adminPassword=''
devPassword=${TASKCOMPASS_DB_PASSWORD:-}

cleanup() {
   unset adminPassword devPassword TASKCOMPASS_DB_PASSWORD MYSQL_PWD
}
trap cleanup EXIT

usage() {
   cat <<'USAGE'
Usage: bash scripts/dev-database.sh setup|inspect|test [options]
  --client PATH       MariaDB/mysql client executable (paths with spaces supported)
  --build-dir PATH    Build directory containing TaskCompassDbDiagnostic[.exe]
  --admin-socket      Linux local socket admin authentication (normally under sudo)
  --admin-user NAME   Administrator account; default root (setup/inspect only)

setup: inspect first; create empty taskcompass_dev and SELECT-only TCP account only
       if neither exists. Prompts for admin password and new password/confirmation.
inspect: read-only administrator inspection of this database/account, no hashes.
test: SELECT-only TCP tests and the existing Qt diagnostic; no administration.

No packages/services are installed/started. No passwords are written to files or
printed. Use a password manager to create/save the new development password.
USAGE
}

fail() {
   printf '%s\n' "$1" >&2
   exit 1
}

while [[ $# -gt 0 ]]; do
   case "$1" in
      --client|--build-dir|--admin-user)
         [[ $# -ge 2 ]] || fail "Missing value for $1"
         case "$1" in
            --client) clientPath=$2 ;;
            --build-dir) buildDirectory=$2 ;;
            --admin-user) adminUser=$2 ;;
         esac
         shift 2 ;;
      --admin-socket) adminSocket=true; shift ;;
      --help|-h) usage; exit 0 ;;
      *) fail "Unknown option: $1" ;;
   esac
done
case "$action" in
   help|--help|-h) usage; exit 0 ;;
   setup|inspect|test) ;;
   *) usage; fail 'Unknown action.' ;;
esac

if [[ -z "$clientPath" ]]; then
   clientPath=$(command -v mariadb || command -v mysql || true)
   if [[ -z "$clientPath" && -n "${MSYSTEM:-}" ]]; then
      for candidate in '/c/Program Files'/MariaDB*/bin/mariadb.exe; do
         if [[ -f "$candidate" ]]; then
            [[ -z "$clientPath" ]] || fail 'Multiple MariaDB installs found; specify --client.'
            clientPath=$candidate
         fi
      done
   fi
fi
[[ -n "$clientPath" ]] || fail 'MariaDB client not found. Specify --client with its executable path.'
[[ -x "$clientPath" ]] || fail 'MariaDB client is not executable; use its full path with --client.'

promptPassword() {
   local prompt=$1
   local variableName=$2
   IFS= read -r -s -p "$prompt" "$variableName" || fail 'Password input cancelled.'
   printf '\n' >&2
}

# Passwords exist only in shell memory and each child process environment; not argv.
# --no-defaults prevents unrelated application/client option files being read.
runClient() {
   local mode=$1
   local sql=$2
   local password=''
   local output=''
   local escapedPassword=${devPassword//\'/\'\'}
   local -a options=(--no-defaults --batch --raw --skip-column-names --connect-timeout=5)
   if [[ "$mode" == admin ]]; then
      password=$adminPassword
      options+=(--user="$adminUser")
      if "$adminSocket"; then
         options+=(--protocol=SOCKET)
      else
         options+=(--protocol=TCP --host="$DATABASE_HOST" --port="$DATABASE_PORT")
      fi
   else
      password=$devPassword
      options+=(--protocol=TCP --host="$DATABASE_HOST" --port="$DATABASE_PORT"
                --user="$DATABASE_USER" "$DATABASE_NAME")
   fi
   if ! output=$(printf '%s\n' "$sql" | MYSQL_PWD="$password" "$clientPath" "${options[@]}" 2>&1); then
      [[ -z "$adminPassword" ]] || output=${output//"$adminPassword"/<redacted>}
      [[ -z "$devPassword" ]] || output=${output//"$devPassword"/<redacted>}
      [[ -z "$escapedPassword" ]] || output=${output//"$escapedPassword"/<redacted>}
      fail "MariaDB failed: $output
Check the running service, TCP 3306, account host match and password; no automatic repair attempted."
   fi
   output=${output//$'\r'/}
   printf '%s' "$output"
}

inspectDatabase() {
   local serverVersion=''
   local databaseNames=''
   serverVersion=$(runClient admin 'SELECT VERSION();')
   databaseNames=$(runClient admin 'SHOW DATABASES;')
   printf 'Server: %s\n' "$serverVersion"
   printf 'Existing database names:\n%s\n' "$databaseNames"
   existingDatabase=$(runClient admin "SELECT SCHEMA_NAME,DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='$DATABASE_NAME';")
   existingAccounts=$(runClient admin "SELECT User,Host FROM mysql.user WHERE User='$DATABASE_USER';")
   printf 'TaskCompass database: %s\n' "${existingDatabase:-(absent)}"
   printf 'TaskCompass account hosts: %s\n' "${existingAccounts:-(absent)}"
   local tableCount=''
   tableCount=$(runClient admin "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='$DATABASE_NAME';")
   printf 'TaskCompass table count: %s\n' "$tableCount"
   local table=''
   local fields=''
   local privileges=''
   local roles=''
   for table in USER_PRIVILEGES SCHEMA_PRIVILEGES TABLE_PRIVILEGES COLUMN_PRIVILEGES; do
      fields='GRANTEE,PRIVILEGE_TYPE,IS_GRANTABLE'
      [[ "$table" == USER_PRIVILEGES ]] || fields="GRANTEE,TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE"
      privileges=$(runClient admin "SELECT $fields FROM information_schema.$table WHERE GRANTEE LIKE CONCAT(CHAR(39),'taskcompass_dev',CHAR(39),'@%');")
      printf '%s: %s\n' "$table" "${privileges:-(none)}"
   done
   roles=$(runClient admin "SELECT Host,User,Role FROM mysql.roles_mapping WHERE User='$DATABASE_USER';")
   printf 'Role assignments: %s\n' "${roles:-(none)}"
}

validateDatabase() {
   if [[ -z "$devPassword" ]]; then
      promptPassword 'TaskCompass development database password: ' devPassword
   fi
   local version=''
   local selected=''
   local account=''
   local tables=''
   version=$(runClient dev 'SELECT VERSION();')
   selected=$(runClient dev 'SELECT DATABASE();')
   account=$(runClient dev 'SELECT CURRENT_USER();')
   tables=$(runClient dev 'SHOW TABLES;')
   [[ "$selected" == "$DATABASE_NAME" ]] || fail 'Wrong database selected.'
   [[ "$account" == "$DATABASE_USER@$DATABASE_HOST" ]] || fail 'Unexpected authenticated host-account.'
   [[ -z "$tables" ]] || fail 'Database is not empty; stop without changing anything.'
   printf 'TCP authenticated: %s; database: %s; server: %s\nSHOW TABLES: empty\n' "$account" "$selected" "$version"
   local globalPrivileges=''
   local schemaPrivileges=''
   local objectPrivileges=''
   globalPrivileges=$(runClient dev "SELECT PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=$GRANTEE_SQL;")
   schemaPrivileges=$(runClient dev "SELECT TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=$GRANTEE_SQL;")
   [[ -z "$globalPrivileges" || "$globalPrivileges" == $'USAGE\tNO' ]] || fail 'Unexpected global privileges; no changes made.'
   [[ "$schemaPrivileges" == $'taskcompass\\_dev\tSELECT\tNO' ]] || fail 'Expected an exact, escaped-database SELECT-only grant; ask the administrator to inspect privileges.'
   for table in TABLE_PRIVILEGES COLUMN_PRIVILEGES; do
      objectPrivileges=$(runClient dev "SELECT PRIVILEGE_TYPE FROM information_schema.$table WHERE GRANTEE=$GRANTEE_SQL;")
      [[ -z "$objectPrivileges" ]] || fail 'Unexpected table/column privileges; no changes made.'
   done
   local grants=''
   local grant=''
   grants=$(runClient dev 'SHOW GRANTS;')
   while IFS= read -r grant; do
      case "$grant" in
         'GRANT USAGE ON *.* TO '*|'GRANT SELECT ON '*) ;;
         *) fail 'Additional direct grants/roles reported; administrator review required. Grant text is not displayed.' ;;
      esac
   done <<< "$grants"
   unset grants grant
   printf 'Reported privileges: SELECT only on the exact development database, no CREATE or GRANT OPTION.\n'
   printf 'No CREATE/DROP permission test was attempted. Existing accounts may also need administrator role inspection.\n'

   if [[ -z "$buildDirectory" ]]; then
      if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
         buildDirectory="$repositoryDirectory/build-ucrt64"
      else
         buildDirectory="$repositoryDirectory/build"
      fi
   fi
   local diagnosticPath="$buildDirectory/TaskCompassDbDiagnostic"
   [[ ! -f "$diagnosticPath.exe" ]] || diagnosticPath+=.exe
   [[ -x "$diagnosticPath" ]] || fail 'Qt diagnostic not found; build first or specify --build-dir (including Debug/Release if applicable).'
   TASKCOMPASS_DB_HOST="$DATABASE_HOST" TASKCOMPASS_DB_PORT="$DATABASE_PORT" \
   TASKCOMPASS_DB_NAME="$DATABASE_NAME" TASKCOMPASS_DB_USER="$DATABASE_USER" \
   TASKCOMPASS_DB_PASSWORD="$devPassword" TASKCOMPASS_DB_ISOLATED=1 \
      "$diagnosticPath" --connect
   tables=$(runClient dev 'SHOW TABLES;')
   [[ -z "$tables" ]] || fail 'Unexpected tables after diagnostic.'
   printf 'CLI and Qt validation passed; database remains empty.\n'
}

if [[ "$action" == test ]]; then
   validateDatabase
   exit 0
fi
if ! "$adminSocket"; then
   promptPassword 'MariaDB administrator password (not your Windows/Linux password): ' adminPassword
fi
existingDatabase=''
existingAccounts=''
inspectDatabase
[[ "$action" != inspect ]] || exit 0
[[ -z "$existingDatabase" && -z "$existingAccounts" ]] || fail 'Database/account already exists. No password or privileges changed. Use test, or review inspect results; do not rerun setup to repair it.'
promptPassword 'New development password from your password manager (at least 20 characters): ' devPassword
[[ ${#devPassword} -ge 20 ]] || fail 'Use a strong password of at least 20 characters, saved privately.'
confirmation=''
promptPassword 'Repeat new development password: ' confirmation
[[ "$devPassword" == "$confirmation" ]] || fail 'Passwords do not match; nothing created.'
unset confirmation
# Session-only SQL mode makes single-quote doubling sufficient even for backslashes.
passwordSql=${devPassword//\'/\'\'}
setupSql="SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';
CREATE DATABASE $DATABASE_NAME CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER '$DATABASE_USER'@'$DATABASE_HOST' IDENTIFIED BY '$passwordSql';
GRANT SELECT ON \`taskcompass\_dev\`.* TO '$DATABASE_USER'@'$DATABASE_HOST';"
runClient admin "$setupSql" >/dev/null
unset passwordSql setupSql
printf 'Created isolated database/account with SELECT only. No password file was written.\n'
validateDatabase
