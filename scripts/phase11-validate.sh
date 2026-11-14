#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Operator-run Phase 1.1 validation. No live operations by default; no repairs.
set +x
set -euo pipefail
umask 077
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$scriptDirectory/.." && pwd)
action=${1:-help}
[[ $# == 0 ]] || shift
clientPath=''
dumpPath=''
buildDirectory=''
backupDirectory=''
approved=false
printPlan=false
adminPassword=''
accountPassword=''
newPassword=''
confirmation=''
serverIdentity=''
trap 'unset adminPassword accountPassword newPassword confirmation MYSQL_PWD TASKCOMPASS_DB_PASSWORD TASKCOMPASS_TEST_DB_PASSWORD' EXIT

fail() { printf 'STOP: %s\n' "$1" >&2; exit 1; }
usage() {
   cat <<'USAGE'
Usage: bash scripts/phase11-validate.sh STEP [options]
Steps (run one at a time; see docs/WindowsPhase1.1.md):
  build               Configure, build, database-free CTest and driver listing.
  inspect             Read-only administrator report, versions and scoped grants.
  integration-setup   Existing guarded fresh test database/account provisioner.
  integration-test    Validate empty target/grants, then approved disposable suite.
  migration-setup     Empty-dev preflight, private baseline dump, fresh migrator.
  migrate             Validate baseline/identity/grants, explicit migration/verify.
  runtime-setup       Verify schema, fresh runtime grants and migrator revocations.
  gui                 Verify as runtime account and launch persistent desktop.
  backup              Private populated dump + source export + checksums.
  restore             Empty-test/privilege preflight, exact-hash approval and import.
  compare             Verify restored schema; compare all five tables/counts.
Options:
  --client PATH       mariadb/mysql executable; auto-detect if omitted.
  --dump PATH         mariadb-dump executable; auto-detect beside client/PATH.
  --build-dir PATH    Default build-ucrt64 on UCRT64, build on Linux.
  --backup-dir PATH   Outside repo; NEW directory for migration-setup/backup,
                      existing directory from that step for migrate/restore/compare.
  --print-plan        No connections/prompts: show SQL/operations for review.
  --approved          Required for provisioning, migration, live tests and restore.
Passwords are prompted by account name, kept in memory only, never saved.
No installation, service changes, automatic reset, retry or DROP cleanup.
USAGE
}
while [[ $# -gt 0 ]]; do
   case "$1" in
      --client|--dump|--build-dir|--backup-dir)
         [[ $# -ge 2 && -n "$2" ]] || fail "Missing value for $1"
         case "$1" in
            --client) clientPath=$2 ;; --dump) dumpPath=$2 ;;
            --build-dir) buildDirectory=$2 ;; --backup-dir) backupDirectory=$2 ;;
         esac
         shift 2 ;;
      --approved) approved=true; shift ;;
      --print-plan) printPlan=true; shift ;;
      *) fail "Unknown option: $1" ;;
   esac
done
case "$action" in
   help|--help|-h) usage; exit 0 ;;
   build|inspect|integration-setup|integration-test|migration-setup|migrate|runtime-setup|gui|backup|restore|compare) ;;
   *) usage; fail 'Unknown step.' ;;
esac
if "$printPlan"; then
   printf 'Step: %s. Loopback TCP 127.0.0.1:3306 only. No operations executed.\n' "$action"
   case "$action" in
      integration-setup) cat "$repositoryDirectory/temp/testdb/setup.template.sql" ;;
      migration-setup) cat "$scriptDirectory/phase11/migration-account.sql" ;;
      runtime-setup) cat "$scriptDirectory/phase11/runtime-account.sql" ;;
      migrate) cat "$repositoryDirectory/migrations/mariadb/001_PersistentTaskFoundation.sql" ;;
      restore) printf 'Import reviewed after-gui.sql into EMPTY taskcompass_integration as taskcompass_test.\nExact artifact hash is confirmed at execution; no --force, reset or retry.\n' ;;
      *) usage ;;
   esac
   exit 0
fi
case "$action" in
   integration-setup|integration-test|migration-setup|migrate|runtime-setup|restore)
      "$approved" || fail 'Review --print-plan and obtain milestone approval, then use --approved.' ;;
esac
if [[ -n "${MSYSTEM:-}" ]]; then
   [[ "$MSYSTEM" == UCRT64 ]] || fail 'Use MSYS2 UCRT64, not MSYS/MINGW64.'
   export PATH="/ucrt64/bin:$PATH"
   export CMAKE_PREFIX_PATH
   CMAKE_PREFIX_PATH=$(cygpath -m /ucrt64)
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}MYSQL_PWD;TASKCOMPASS_DB_PASSWORD;TASKCOMPASS_TEST_DB_PASSWORD"
fi
if [[ -z "$buildDirectory" ]]; then
   buildDirectory="$repositoryDirectory/build"
   [[ "${MSYSTEM:-}" != UCRT64 ]] || buildDirectory="$repositoryDirectory/build-ucrt64"
fi
exe() {
   executable="$buildDirectory/$1"
   [[ ! -f "$executable.exe" ]] || executable+=.exe
   [[ -x "$executable" ]] || fail "Build $1 first: $buildDirectory"
}
if [[ "$action" == build ]]; then
   cmake --version
   g++ --version
   if command -v qmake6 >/dev/null; then
      qmake6 -query QT_VERSION
   elif command -v qmake >/dev/null; then
      qmake -query QT_VERSION
   else
      printf 'Qt version query unavailable; include installed Qt package version in your report.\n'
   fi
   cmake -S "$repositoryDirectory" -B "$buildDirectory" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
   cmake --build "$buildDirectory" --parallel 2
   ctest --test-dir "$buildDirectory" --output-on-failure
   exe TaskCompassDbDiagnostic
   "$executable"
   printf 'PASS: build and database-free tests; no live validation claimed.\n'
   exit 0
fi
if [[ -z "$clientPath" ]]; then
   clientPath=$(command -v mariadb || command -v mysql || true)
   if [[ -z "$clientPath" && "${MSYSTEM:-}" == UCRT64 ]]; then
      for candidate in '/c/Program Files'/MariaDB*/bin/mariadb.exe; do
         [[ -f "$candidate" ]] || continue
         [[ -z "$clientPath" ]] || fail 'Multiple MariaDB clients; specify --client.'
         clientPath=$candidate
      done
   fi
fi
[[ -n "$clientPath" && -x "$clientPath" ]] || fail 'Specify --client with a MariaDB executable.'
if [[ "$action" == integration-setup ]]; then
   exec bash "$repositoryDirectory/temp/testdb/setup.sh" --apply --client "$clientPath"
fi

prompt() {
   IFS= read -r -s -p "$1: " "$2" || fail 'Password input cancelled.'
   printf '\n' >&2
   [[ -n "${!2}" ]] || fail 'Empty password; nothing attempted.'
}
confirm() {
   IFS= read -r -p "Type $1 to confirm the approved operation: " confirmation || fail 'Cancelled.'
   [[ "$confirmation" == "$1" ]] || fail 'Confirmation mismatch; nothing changed.'
   unset confirmation
}
redact() {
   local line=''
   while IFS= read -r line || [[ -n "$line" ]]; do
      local secret=''
      for secret in "$adminPassword" "$accountPassword" "$newPassword"; do
         [[ -z "$secret" ]] || line=${line//"$secret"/<redacted>}
         secret=${secret//\'/\'\'}
         [[ -z "$secret" ]] || line=${line//"$secret"/<redacted>}
      done
      line=${line//$'\r'/}
      if [[ "$line" == 'WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.' ]]; then
         printf 'MariaDB client password-option notice (MYSQL_PWD is supplied); TLS verification settings unchanged.\n' >&2
      else
         printf '%s\n' "$line" >&2
      fi
   done
}
client() {
   local user=$1 password=$2 database=$3 sql=$4
   local -a options=(--no-defaults --protocol=TCP --host=127.0.0.1 --port=3306
                     --user="$user" --connect-timeout=5 --default-character-set=utf8mb4 --batch --skip-column-names)
   [[ "${5:-}" != raw ]] || options+=(--raw)
   [[ -z "$database" ]] || options+=("$database")
   printf '%s\n' "$sql" | MYSQL_PWD="$password" "$clientPath" "${options[@]}" 2> >(redact)
}
admin() { client root "$adminPassword" '' "$1"; }
query() {
   local result=''
   result=$(client "$1" "$2" "$3" "$4" raw) || fail 'Read-only query failed; no automatic repair.'
   printf '%s' "${result//$'\r'/}"
}
adminQuery() { query root "$adminPassword" '' "$1"; }
adminLogin() {
   prompt 'MariaDB administrator password (root, not Windows/Linux login)' adminPassword
   serverIdentity=$(adminQuery 'SELECT @@hostname,VERSION(),@@port;')
   [[ "$serverIdentity" == *$'\t3306' ]] || fail 'Unexpected server port.'
   printf 'Server: %s\nPlatform: %s / %s\n' "$serverIdentity" "$(uname -s)" "${MSYSTEM:-Linux}"
}
identity() {
   local expected="$3"$'\t'"$1@127.0.0.1" actual=''
   actual=$(query "$1" "$2" "$3" 'SELECT DATABASE(),CURRENT_USER();')
   [[ "$actual" == "$expected" ]] || fail 'Database/authenticated account differs from fixed target.'
}
emptySchema() {
   local state=''
   state=$(adminQuery "SELECT (SELECT COUNT(*) FROM information_schema.SCHEMATA WHERE SCHEMA_NAME='$1' AND DEFAULT_CHARACTER_SET_NAME='utf8mb4' AND DEFAULT_COLLATION_NAME='utf8mb4_unicode_ci'),(SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA='$1'),(SELECT COUNT(*) FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA='$1'),(SELECT COUNT(*) FROM information_schema.TRIGGERS WHERE TRIGGER_SCHEMA='$1'),(SELECT COUNT(*) FROM information_schema.EVENTS WHERE EVENT_SCHEMA='$1');")
   [[ "$state" == $'1\t0\t0\t0\t0' ]] || fail "$1 must exist with expected charset/collation and be entirely empty. No reset performed."
}
absentAccount() {
   [[ "$(adminQuery "SELECT COUNT(*) FROM mysql.user WHERE User='$1';")" == 0 ]] || fail "Account $1 already exists on some host. Stop; no overwrite/reuse."
}
# Exact direct grants, no roles. PUBLIC is separately reviewed for both target names.
grants() {
   local user=$1 state=$2 scope=taskcompass_dev expected='' actual='' grantee=''
   [[ "$user" != taskcompass_test ]] || scope=taskcompass_integration
   [[ "$(adminQuery "SELECT Host FROM mysql.user WHERE User='$user';")" == 127.0.0.1 ]] || fail "Unexpected hosts for $user."
   grantee="CONCAT(CHAR(39),'$user',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39))"
   [[ "$(adminQuery "SELECT COUNT(*) FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=$grantee AND (PRIVILEGE_TYPE<>'USAGE' OR IS_GRANTABLE<>'NO');")" == 0 ]] || fail 'Unexpected global privilege.'
   [[ "$(adminQuery "SELECT COUNT(*) FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE=$grantee;")" == 0 ]] || fail 'Unexpected column privileges.'
   [[ "$(adminQuery "SELECT COUNT(*) FROM mysql.roles_mapping WHERE User='$user';")" == 0 ]] || fail 'Explicit roles require separate review.'
   actual=$(adminQuery "SELECT CONCAT('schema|',TABLE_SCHEMA,'|',PRIVILEGE_TYPE,'|',IS_GRANTABLE) FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=$grantee UNION ALL SELECT CONCAT('table|',LOWER(TABLE_NAME),'|',PRIVILEGE_TYPE,'|',IS_GRANTABLE) FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=$grantee AND TABLE_SCHEMA='$scope' UNION ALL SELECT 'unexpected-schema' FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=$grantee AND TABLE_SCHEMA<>'$scope';")
   case "$state" in
      diagnostic|readonly) expected=$'schema|taskcompass\\_dev|SELECT|NO' ;;
      migration) expected=$'schema|taskcompass\\_dev|SELECT|NO\nschema|taskcompass\\_dev|CREATE|NO\ntable|dataset|CREATE|NO\ntable|dataset|INSERT|NO\ntable|schemamigration|CREATE|NO\ntable|schemamigration|INSERT|NO' ;;
      runtime) expected=$'table|task|SELECT|NO\ntable|task|INSERT|NO\ntable|task|UPDATE|NO\ntable|commandrecord|SELECT|NO\ntable|commandrecord|INSERT|NO\ntable|localchange|SELECT|NO\ntable|localchange|INSERT|NO\ntable|dataset|SELECT|NO\ntable|schemamigration|SELECT|NO' ;;
      test)
         for right in SELECT INSERT UPDATE CREATE DROP ALTER; do
            expected+="schema|taskcompass\\_integration|$right|NO"$'\n'
         done
         expected=${expected%$'\n'} ;;
      *) fail 'Unknown grant policy.' ;;
   esac
   [[ "$(printf '%s\n' "$actual" | LC_ALL=C sort)" == "$(printf '%s\n' "$expected" | LC_ALL=C sort)" ]] || fail "Unexpected direct grants for $user; no automatic privilege changes."
   printf 'PASS: exact direct grants and no explicit roles for %s.\n' "$user"
}
publicPolicy() {
   local version='' grantText='' DATABASE_NAME='' decodedIdentifier=''
   local publicGrantCount=0 unrelatedGrantCount=0
   local -a grantRoleQueue=()
   local -A seenGrantRoles=()
   version=$(adminQuery 'SELECT VERSION();')
   [[ "$version" =~ ^([0-9]+)\.([0-9]+) ]] || fail 'Unknown server version.'
   if (( BASH_REMATCH[1] < 10 || (BASH_REMATCH[1] == 10 && BASH_REMATCH[2] < 11) )); then
      return
   fi
   source "$scriptDirectory/validate-grants.sh"
   grantText=$(adminQuery 'SHOW GRANTS FOR PUBLIC;')
   [[ -n "$grantText" ]] || fail 'Cannot establish PUBLIC policy.'
   for DATABASE_NAME in taskcompass_dev taskcompass_integration; do
      parseGrantBatch PUBLIC "$grantText"
   done
   [[ ${#grantRoleQueue[@]} == 0 ]] || fail 'PUBLIC has inherited roles; separate review required.'
   printf 'PASS: PUBLIC has no write/DDL authority covering either TaskCompass schema.\n'
}
setConfig() {
   if [[ "$1" == taskcompass_test ]]; then
      export TASKCOMPASS_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_TEST_DB_PORT=3306
      export TASKCOMPASS_TEST_DB_NAME=taskcompass_integration TASKCOMPASS_TEST_DB_USER=taskcompass_test TASKCOMPASS_TEST_DB_ISOLATED=1
      export TASKCOMPASS_TEST_DB_PASSWORD="$accountPassword"
   else
      export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
      export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER="$1" TASKCOMPASS_DB_ISOLATED=1
      export TASKCOMPASS_DB_PASSWORD="$accountPassword"
   fi
}
verify() {
   local user=$1 output=''
   exe TaskCompassMigrate
   prompt "Password for $user" accountPassword
   setConfig "$user"
   local -a options=(--verify)
   [[ "$user" != taskcompass_test ]] || options+=(--integration)
   output=$("$executable" "${options[@]}") || fail 'Schema verification failed.'
   printf '%s\n' "${output//$'\r'/}"
   [[ "$output" =~ dataset[[:space:]]([0-9a-f-]{36}) ]] || fail 'Missing verified dataset UUID.'
   datasetId=${BASH_REMATCH[1]}
}
newDirectory() {
   [[ -n "$backupDirectory" ]] || fail '--backup-dir must name a new private directory outside the repo.'
   mkdir -p -- "$(dirname -- "$backupDirectory")"
   local parent=''
   parent=$(cd -- "$(dirname -- "$backupDirectory")" && pwd -P)
   [[ "$parent/" != "$repositoryDirectory/"* ]] || fail 'Backups/exports must be outside the repository.'
   backupDirectory="$parent/$(basename -- "$backupDirectory")"
   mkdir -- "$backupDirectory" || fail 'Backup directory exists; never overwrite. Choose a new name.'
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
      command -v powershell.exe >/dev/null || fail 'PowerShell required to restrict backup ACL.'
      TASKCOMPASS_PRIVATE_DIRECTORY=$(cygpath -w "$backupDirectory") powershell.exe -NoProfile -NonInteractive -Command '
         $ErrorActionPreference="Stop"
         $sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User
         $acl=New-Object System.Security.AccessControl.DirectorySecurity
         $acl.SetOwner($sid)
         $acl.SetAccessRuleProtection($true,$false)
         $rule=New-Object System.Security.AccessControl.FileSystemAccessRule($sid,"FullControl","ContainerInherit,ObjectInherit","None","Allow")
         $acl.AddAccessRule($rule)
         Set-Acl -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY -AclObject $acl
         $actual=Get-Acl -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY
         $rules=$actual.GetAccessRules($true,$true,[System.Security.Principal.SecurityIdentifier])
         if (!$actual.AreAccessRulesProtected -or $rules.Count -ne 1 -or
             $rules[0].IdentityReference.Value -ne $sid.Value -or
             $rules[0].FileSystemRights -ne [System.Security.AccessControl.FileSystemRights]::FullControl -or
             $rules[0].AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) {
            throw "Private backup ACL verification failed"
         }
      ' || fail 'Cannot establish private Windows backup ACL; no dump made.'
   fi
}
existingDirectory() {
   [[ -n "$backupDirectory" && -d "$backupDirectory" && ! -L "$backupDirectory" ]] || fail 'Specify the existing backup directory from the earlier step.'
   backupDirectory=$(cd -- "$backupDirectory" && pwd -P)
   [[ "$backupDirectory/" != "$repositoryDirectory/"* ]] || fail 'Backup must be outside repository.'
}
dumpDatabase() {
   if [[ -z "$dumpPath" ]]; then
      local candidate="$(dirname -- "$clientPath")/mariadb-dump"
      [[ ! -x "$candidate.exe" ]] || candidate+=.exe
      if [[ -x "$candidate" ]]; then dumpPath=$candidate
      else dumpPath=$(command -v mariadb-dump || command -v mysqldump || true); fi
   fi
   [[ -n "$dumpPath" && -x "$dumpPath" ]] || fail 'Specify --dump with mariadb-dump executable.'
   local file="$backupDirectory/$1"
   [[ "${MSYSTEM:-}" != UCRT64 ]] || file=$(cygpath -m "$file")
   MYSQL_PWD="$accountPassword" "$dumpPath" --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
      --user=taskcompass_dev --default-character-set=utf8mb4 --single-transaction \
      --skip-lock-tables --no-tablespaces --skip-triggers --skip-add-drop-table \
      --skip-add-locks --skip-disable-keys --no-create-db --hex-blob \
      --result-file="$file" taskcompass_dev 2> >(redact) || fail 'Backup failed; preserve artifact, do not use it.'
   [[ -s "$backupDirectory/$1" ]] || fail 'Backup is empty.'
   (cd -- "$backupDirectory" && sha256sum "$1" > "$1.sha256")
   printf 'PASS: backup exit 0, nonempty, hash recorded: %s\n' "$backupDirectory/$1"
}
readNewPassword() {
   prompt "New $1 password from password manager (20+ characters)" newPassword
   [[ ${#newPassword} -ge 20 ]] || fail 'Password too short.'
   prompt "Repeat new $1 password" confirmation
   [[ "$confirmation" == "$newPassword" ]] || fail 'Passwords differ.'
   unset confirmation
}
applyTemplate() {
   local sql=''
   sql=$(cat "$scriptDirectory/phase11/$1-account.sql")
   local literal="'${newPassword//\'/\'\'}'"
   sql=${sql/'{{PASSWORD}}'/"$literal"}
   admin "$sql" >/dev/null || fail 'Provisioning/revocation failed; preserve partial state, do not retry.'
   unset newPassword
}
comparisonSql="SELECT 'Dataset',COUNT(*) FROM Dataset
UNION ALL SELECT 'Task',COUNT(*) FROM Task
UNION ALL SELECT 'CommandRecord',COUNT(*) FROM CommandRecord
UNION ALL SELECT 'LocalChange',COUNT(*) FROM LocalChange
UNION ALL SELECT 'SchemaMigration',COUNT(*) FROM SchemaMigration;
SELECT * FROM Dataset ORDER BY id;
SELECT * FROM Task ORDER BY id;
SELECT * FROM CommandRecord ORDER BY id;
SELECT * FROM LocalChange ORDER BY id;
SELECT * FROM SchemaMigration ORDER BY version;"

case "$action" in
   inspect)
      adminLogin
      admin "SELECT SCHEMA_NAME,DEFAULT_CHARACTER_SET_NAME,DEFAULT_COLLATION_NAME FROM information_schema.SCHEMATA WHERE SCHEMA_NAME IN ('taskcompass_dev','taskcompass_integration'); SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE FROM information_schema.TABLES WHERE TABLE_SCHEMA IN ('taskcompass_dev','taskcompass_integration'); SELECT User,Host FROM mysql.user WHERE User IN ('taskcompass_dev','taskcompass_test','taskcompass_migrate','taskcompass_app');"
      for user in taskcompass_dev taskcompass_test taskcompass_migrate taskcompass_app; do
         grantee="CONCAT(CHAR(39),'$user',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39))"
         printf 'Scoped privilege report for %s (authentication data omitted):\n' "$user"
         admin "SELECT PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.USER_PRIVILEGES WHERE GRANTEE=$grantee; SELECT TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES WHERE GRANTEE=$grantee; SELECT TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE=$grantee; SELECT TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.COLUMN_PRIVILEGES WHERE GRANTEE=$grantee; SELECT Host,User,Role FROM mysql.roles_mapping WHERE User='$user';"
      done
      publicPolicy
      printf 'Read-only report; existing accounts/databases are not overwritten.\n' ;;
   integration-test)
      confirm 'TEST taskcompass_integration WITH DISPOSABLE RESET'
      adminLogin; emptySchema taskcompass_integration; grants taskcompass_test test; publicPolicy
      bash "$repositoryDirectory/temp/testdb/test.sh" --run --allow-reset --build-dir "$buildDirectory" ;;
   migration-setup)
      confirm 'PROVISION taskcompass_migrate'
      adminLogin; emptySchema taskcompass_dev; absentAccount taskcompass_migrate; absentAccount taskcompass_app
      grants taskcompass_dev diagnostic; publicPolicy
      newDirectory
      prompt 'Password for taskcompass_dev (diagnostic/backup account)' accountPassword
      identity taskcompass_dev "$accountPassword" taskcompass_dev
      dumpDatabase before-migration.sql
      printf '%s\n' "$serverIdentity" > "$backupDirectory/server.txt"
      readNewPassword taskcompass_migrate
      applyTemplate migration
      grants taskcompass_migrate migration
      printf 'PASS: baseline backup and fresh migration account. Next: migrate with this --backup-dir.\n' ;;
   migrate)
      confirm 'MIGRATE taskcompass_dev'
      existingDirectory
      (cd -- "$backupDirectory" && sha256sum --check before-migration.sql.sha256) || fail 'Baseline backup checksum mismatch.'
      [[ -s "$backupDirectory/before-migration.sql" ]] || fail 'Missing baseline backup.'
      adminLogin; emptySchema taskcompass_dev; grants taskcompass_migrate migration; publicPolicy
      [[ "$(cat "$backupDirectory/server.txt")" == "$serverIdentity" ]] || fail 'Baseline server differs.'
      prompt 'Password for taskcompass_migrate' accountPassword
      setConfig taskcompass_migrate; exe TaskCompassMigrate
      "$executable" --apply || fail 'Migration failed; stop, preserve partial DDL, no retry.'
      "$executable" --verify || fail 'Post-migration verification failed.'
      printf 'PASS: migration and verification.\n' ;;
   runtime-setup)
      confirm 'PROVISION taskcompass_app AND REVOKE migrator writes'
      verify taskcompass_migrate
      adminLogin; absentAccount taskcompass_app; grants taskcompass_migrate migration; publicPolicy
      readNewPassword taskcompass_app; applyTemplate runtime
      grants taskcompass_app runtime; grants taskcompass_migrate readonly; grants taskcompass_dev diagnostic
      printf 'PASS: runtime grants and migrator write revocation.\n' ;;
   gui)
      adminLogin; grants taskcompass_app runtime; grants taskcompass_migrate readonly; grants taskcompass_dev diagnostic; publicPolicy
      verify taskcompass_app
      exe TaskCompass; "$executable"
      printf 'GUI exited successfully. Manual checklist is still required; run gui again for restart persistence.\n' ;;
   backup)
      confirm 'ALL TASKCOMPASS WRITERS CLOSED'
      verify taskcompass_app
      newDirectory
      printf '%s\n' "$datasetId" > "$backupDirectory/dataset.txt"
      prompt 'Password for taskcompass_dev (diagnostic/backup account)' accountPassword
      identity taskcompass_dev "$accountPassword" taskcompass_dev
      query taskcompass_dev "$accountPassword" taskcompass_dev 'SELECT @@hostname,VERSION(),@@port;' > "$backupDirectory/server.txt"
      client taskcompass_dev "$accountPassword" taskcompass_dev "$comparisonSql" > "$backupDirectory/source.tsv" || fail 'Source export failed.'
      dumpDatabase after-gui.sql
      client taskcompass_dev "$accountPassword" taskcompass_dev "$comparisonSql" > "$backupDirectory/source-after.tsv" || fail 'Post-backup source export failed.'
      cmp -s "$backupDirectory/source.tsv" "$backupDirectory/source-after.tsv" || fail 'Source changed during backup; preserve files and take a fresh quiescent backup.'
      (cd -- "$backupDirectory" && sha256sum source.tsv > source.tsv.sha256)
      printf 'PASS: consistent source exports and populated backup. Privately inspect after-gui.sql before restore approval.\n' ;;
   restore)
      existingDirectory
      (cd -- "$backupDirectory" && sha256sum --check after-gui.sql.sha256 source.tsv.sha256) || fail 'Backup/source checksum mismatch.'
      hash=$(sha256sum "$backupDirectory/after-gui.sql"); hash=${hash%% *}
      confirm "RESTORE taskcompass_integration $hash"
      confirm 'DUMP REVIEWED; WRITERS CLOSED'
      adminLogin; emptySchema taskcompass_integration; grants taskcompass_test test; publicPolicy
      [[ "$(cat "$backupDirectory/server.txt")" == "$serverIdentity" ]] || fail 'Backup server differs; cross-server/version restore needs separate review.'
      prompt 'Password for taskcompass_test' accountPassword
      identity taskcompass_test "$accountPassword" taskcompass_integration
      MYSQL_PWD="$accountPassword" "$clientPath" --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
         --user=taskcompass_test --default-character-set=utf8mb4 taskcompass_integration \
         < "$backupDirectory/after-gui.sql" 2> >(redact) || fail 'Import failed; preserve partial state. Do not rerun/reset.'
      printf 'PASS: import exit 0. Next: compare, not integration-test.\n' ;;
   compare)
      existingDirectory
      (cd -- "$backupDirectory" && sha256sum --check after-gui.sql.sha256 source.tsv.sha256) || fail 'Backup/source checksum mismatch.'
      confirm 'ALL TASKCOMPASS WRITERS CLOSED'
      verify taskcompass_test
      [[ "$(cat "$backupDirectory/dataset.txt")" == "$datasetId" ]] || fail 'Restored dataset differs.'
      [[ ! -e "$backupDirectory/restored.tsv" && ! -e "$backupDirectory/current-source.tsv" ]] || fail 'Comparison evidence already exists; do not overwrite.'
      client taskcompass_test "$accountPassword" taskcompass_integration "$comparisonSql" > "$backupDirectory/restored.tsv" || fail 'Restore export failed.'
      prompt 'Password for taskcompass_dev (diagnostic/backup account)' accountPassword
      identity taskcompass_dev "$accountPassword" taskcompass_dev
      client taskcompass_dev "$accountPassword" taskcompass_dev "$comparisonSql" > "$backupDirectory/current-source.tsv" || fail 'Current source export failed.'
      sha256sum "$backupDirectory/source.tsv" "$backupDirectory/restored.tsv" "$backupDirectory/current-source.tsv"
      cmp -s "$backupDirectory/source.tsv" "$backupDirectory/restored.tsv" || fail 'Restored data differs; no automatic repair.'
      cmp -s "$backupDirectory/source.tsv" "$backupDirectory/current-source.tsv" || fail 'Development changed after backup; investigate, do not reset.'
      printf 'PASS: schema/dataset and all five tables/counts match backup snapshot and current source.\n' ;;
esac
