#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
export MOCK_STATE_DIRECTORY="$temporaryDirectory"
mkdir -p "$temporaryDirectory/client with spaces" "$temporaryDirectory/build"
clientPath="$temporaryDirectory/client with spaces/mariadb"
cat > "$clientPath" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
sql=$(cat)
[[ "$1" == --no-defaults ]]
for argument in "$@"; do
   [[ "$argument" != *'fixture-password'* ]] || exit 91
done
if [[ "${MOCK_FAILURE:-0}" == 1 ]]; then
   printf 'connection failed for password %s\n' "${MYSQL_PWD:-}" >&2
   exit 1
fi
case "$sql" in
   'SELECT VERSION();') printf '10.5.mock\r\n' ;;
   'SELECT DATABASE();') printf 'taskcompass_dev\r\n' ;;
   'SELECT CURRENT_USER();') printf 'taskcompass_dev@127.0.0.1\r\n' ;;
   'SHOW TABLES;') [[ "${MOCK_TABLES:-0}" != 1 ]] || printf 'unexpected_table\n' ;;
   'SHOW DATABASES;') printf 'information_schema\nmysql\n' ;;
   'SHOW GRANTS;')
      printf "GRANT USAGE ON *.* TO 'taskcompass_dev'@'127.0.0.1' IDENTIFIED BY PASSWORD '*FAKE-HASH'\n"
      printf "GRANT SELECT ON \`taskcompass\\_dev\`.* TO 'taskcompass_dev'@'127.0.0.1'\n"
      [[ "${MOCK_ROLE:-0}" != 1 ]] || printf "GRANT 'unexpected_role' TO 'taskcompass_dev'@'127.0.0.1'\n" ;;
   'SELECT SCHEMA_NAME,'*)
      [[ "${MOCK_EXISTING:-0}" != 1 ]] || printf 'taskcompass_dev\tutf8mb4\tutf8mb4_unicode_ci\n' ;;
   'SELECT User,Host'*)
      [[ "${MOCK_EXISTING:-0}" != 1 ]] || printf 'taskcompass_dev\t127.0.0.1\n' ;;
   'SELECT COUNT(*)'*) printf '0\n' ;;
   'SELECT GRANTEE,'*|'SELECT Host,User,Role'*) ;;
   'SELECT PRIVILEGE_TYPE,IS_GRANTABLE'*) printf 'USAGE\tNO\n' ;;
   'SELECT TABLE_SCHEMA,PRIVILEGE_TYPE'*) printf 'taskcompass\_dev\tSELECT\tNO\r\n' ;;
   'SELECT PRIVILEGE_TYPE FROM'*) ;;
   'SET SESSION sql_mode='*)
      [[ "$sql" == *"NO_BACKSLASH_ESCAPES"* ]]
      [[ "$sql" == *"CREATE USER 'taskcompass_dev'@'127.0.0.1'"* ]]
      [[ "$sql" == *'GRANT SELECT ON `taskcompass\_dev`.*'* ]]
      [[ "$sql" != *'CREATE TABLE'* ]]
      [[ "$sql" == *"IDENTIFIED BY 'fixture-password-with-quote''and\\slash';"* ]]
      touch "$MOCK_STATE_DIRECTORY/created" ;;
   *) printf 'Unexpected mock query\n' >&2; exit 92 ;;
esac
exit 0
CLIENT
cat > "$temporaryDirectory/build/TaskCompassDbDiagnostic" <<'DIAGNOSTIC'
#!/usr/bin/env bash
set -euo pipefail
[[ "$1" == --connect ]]
[[ "$TASKCOMPASS_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_DB_PORT" == 3306 ]]
[[ "$TASKCOMPASS_DB_NAME" == taskcompass_dev && "$TASKCOMPASS_DB_USER" == taskcompass_dev ]]
[[ "$TASKCOMPASS_DB_ISOLATED" == 1 && -n "$TASKCOMPASS_DB_PASSWORD" ]]
printf 'Connection succeeded. Server version: mock\n'
DIAGNOSTIC
chmod +x "$clientPath" "$temporaryDirectory/build/TaskCompassDbDiagnostic"
scriptPath="$repositoryDirectory/scripts/dev-database.sh"
export TASKCOMPASS_DB_PASSWORD=fixture-password-for-testing-only
output=$(bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ "$output" == *'CLI and Qt validation passed'* && "$output" != *'fixture-password'* && "$output" != *'FAKE-HASH'* ]]
printf 'PASS: read-only testing, CRLF normalization, private grants and paths with spaces.\n'
if output=$(MOCK_FAILURE=1 bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1); then
   printf 'FAIL: client failure was swallowed.\n' >&2; exit 1
fi
[[ "$output" == *'<redacted>'* && "$output" != *'fixture-password'* ]]
printf 'PASS: connection failure stops execution and redacts passwords.\n'
if MOCK_TABLES=1 bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" >/dev/null 2>&1; then
   printf 'FAIL: nonempty database accepted.\n' >&2; exit 1
fi
if output=$(MOCK_ROLE=1 bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1); then
   printf 'FAIL: extra role accepted.\n' >&2; exit 1
fi
[[ "$output" != *'FAKE-HASH'* ]]
printf 'PASS: nonempty database and extra grants/roles rejected.\n'
if printf 'admin-fixture-password\n' | MOCK_EXISTING=1 bash "$scriptPath" setup --client "$clientPath" >/dev/null 2>&1; then
   printf 'FAIL: existing resources accepted for setup.\n' >&2; exit 1
fi
[[ ! -e "$temporaryDirectory/created" ]]
if printf 'admin-fixture-password\nshort\n' | bash "$scriptPath" setup --client "$clientPath" >/dev/null 2>&1; then
   printf 'FAIL: short password accepted.\n' >&2; exit 1
fi
[[ ! -e "$temporaryDirectory/created" ]]
if printf 'admin-fixture-password\n' | bash "$scriptPath" setup --client "$clientPath" >/dev/null 2>&1; then
   printf 'FAIL: cancelled password input accepted.\n' >&2; exit 1
fi
if printf 'admin-fixture-password\n' | MOCK_FAILURE=1 bash "$scriptPath" setup --client "$clientPath" >/dev/null 2>&1; then
   printf 'FAIL: admin inspection failure accepted.\n' >&2; exit 1
fi
[[ ! -e "$temporaryDirectory/created" ]]
printf 'PASS: existing resources, cancelled/rejected password and failed inspection cannot trigger provisioning.\n'
newPassword="fixture-password-with-quote'and\\slash"
output=$(printf '%s\n%s\n%s\n' admin-fixture-password "$newPassword" "$newPassword" |
   bash "$scriptPath" setup --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ -e "$temporaryDirectory/created" && "$output" == *'CLI and Qt validation passed'* ]]
[[ "$output" != *'fixture-password'* && "$output" != *'FAKE-HASH'* ]]
printf 'PASS: fresh setup uses escaped password SQL, exact grant and restricted validation.\n'
bash -ec 'source "$1" --no-password; [[ "$TASKCOMPASS_DB_NAME" == taskcompass_dev ]]; clearTaskCompassDatabasePassword; [[ ! -v TASKCOMPASS_DB_PASSWORD ]]' bash "$repositoryDirectory/scripts/dev-environment.sh" >/dev/null
if bash "$repositoryDirectory/scripts/dev-environment.sh" >/dev/null 2>&1; then
   printf 'FAIL: executing environment helper should require sourcing.\n' >&2; exit 1
fi
printf 'PASS: environment sourcing and credential cleanup.\n'
