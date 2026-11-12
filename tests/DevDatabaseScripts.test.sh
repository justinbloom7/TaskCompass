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
if [[ "${MOCK_WARNING:-0}" == 1 ]]; then
   printf 'WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.\r\n' >&2
   printf 'WARNING: redaction fixture %s\n' "${MYSQL_PWD:-}" >&2
fi
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
      case "${MOCK_ROLE:-0}" in
         1|writer) printf "GRANT \`writer\` TO 'taskcompass_dev'@'127.0.0.1'\n" ;;
         reader|nested|unreadable|publicRole) printf "GRANT \`reader\` TO 'taskcompass_dev'@'127.0.0.1'\n" ;;
         multi) printf "GRANT \`reader\`, \`helper\` TO 'taskcompass_dev'@'127.0.0.1'\n" ;;
      esac
      case "${MOCK_PUBLIC:-0}" in
         test) printf 'GRANT ALL PRIVILEGES ON `test`.* TO PUBLIC\nGRANT ALL PRIVILEGES ON `test\_%%`.* TO `PUBLIC`\n' ;;
         target) printf 'GRANT INSERT ON `taskcompass\_dev`.* TO PUBLIC\n' ;;
         wildcard) printf 'GRANT UPDATE ON `taskcompass%%`.* TO PUBLIC\n' ;;
         wildcardUnderscore) printf 'GRANT CREATE ON `taskcompass_dev`.* TO PUBLIC\n' ;;
         global) printf 'GRANT CREATE ON *.* TO PUBLIC\n' ;;
         ddl) printf 'GRANT ALTER ON `taskcompass\_dev`.* TO PUBLIC\n' ;;
         grantOption) printf 'GRANT SELECT ON `taskcompass\_dev`.* TO PUBLIC WITH GRANT OPTION\n' ;;
         unknown) printf 'DENY UPDATE ON *.* TO PUBLIC\n' ;;
      esac ;;
   'SELECT ROLE_NAME,IS_GRANTABLE FROM information_schema.APPLICABLE_ROLES;')
      case "${MOCK_ROLE:-0}" in
         1|writer) printf 'writer\tNO\n' ;;
         reader|nested|unreadable|multi|publicRole) printf 'reader\tNO\n' ;;
         inactiveWriter) printf 'writer\tNO\n' ;;
      esac ;;
   'SELECT ROLE_NAME FROM information_schema.ENABLED_ROLES WHERE ROLE_NAME IS NOT NULL;')
      [[ "${MOCK_ROLE:-0}" != reader ]] || printf 'reader\n' ;;
   'SET SESSION sql_mode='*'SHOW GRANTS FOR `reader`;')
      if [[ "${MOCK_ROLE:-0}" == unreadable ]]; then
         printf 'Role grants not visible\n' >&2; exit 1
      fi
      printf 'GRANT SELECT ON `taskcompass\_dev`.* TO `reader`\n'
      [[ "${MOCK_ROLE:-0}" != nested ]] || printf 'GRANT `writer` TO `reader`\n'
      [[ "${MOCK_ROLE:-0}" != publicRole ]] || printf 'GRANT DROP ON *.* TO PUBLIC\n' ;;
   'SET SESSION sql_mode='*'SHOW GRANTS FOR `helper`;')
      printf 'GRANT USAGE ON *.* TO `helper`\n' ;;
   'SET SESSION sql_mode='*'SHOW GRANTS FOR `writer`;')
      printf 'GRANT DELETE ON `taskcompass\_dev`.* TO `writer`\n' ;;
   'SELECT SCHEMA_NAME,'*)
      [[ "${MOCK_EXISTING:-0}" != 1 ]] || printf 'taskcompass_dev\tutf8mb4\tutf8mb4_unicode_ci\n' ;;
   'SELECT User,Host'*)
      [[ "${MOCK_EXISTING:-0}" != 1 ]] || printf 'taskcompass_dev\t127.0.0.1\n' ;;
   'SELECT COUNT(*)'*) printf '0\n' ;;
   'SELECT GRANTEE,'*|'SELECT Host,User,Role'*) ;;
   'SELECT PRIVILEGE_TYPE,IS_GRANTABLE'*)
      printf 'USAGE\tNO\n'
      [[ "${MOCK_DIRECT_GLOBAL:-0}" != 1 ]] || printf 'FILE\tNO\n' ;;
   'SELECT TABLE_SCHEMA,PRIVILEGE_TYPE'*)
      printf 'taskcompass\_dev\tSELECT\tNO\r\n'
      [[ "${MOCK_DIRECT_WRITE:-0}" != 1 ]] || printf 'taskcompass\_dev\tINSERT\tNO\n' ;;
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
touch "$MOCK_STATE_DIRECTORY/qt-ran"
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
warningOutput=$(printf '%s\n%s\n%s\n' admin-fixture-password "$newPassword" "$newPassword" |
   MOCK_WARNING=1 bash "$scriptPath" setup --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ "$warningOutput" == *'CLI and Qt validation passed'* ]]
[[ "$warningOutput" == *'TaskCompass database: (absent)'* && "$warningOutput" == *'TaskCompass account hosts: (absent)'* ]]
[[ "$warningOutput" == *'Server: 10.5.mock'* && "$warningOutput" == *'WARNING: option --ssl-verify-server-cert'* ]]
[[ "$warningOutput" != *'fixture-password'* && "$warningOutput" != *'FAKE-HASH'* ]]
warningOutput=$(MOCK_WARNING=1 bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ "$warningOutput" == *'CLI and Qt validation passed'* && "$warningOutput" != *'fixture-password'* ]]
if printf 'admin-fixture-password\n' | MOCK_EXISTING=1 MOCK_WARNING=1 bash "$scriptPath" setup --client "$clientPath" >/dev/null 2>&1; then
   printf 'FAIL: warnings bypassed existing-resource protection.\n' >&2; exit 1
fi
printf 'PASS: successful client stderr warnings stay visible/redacted without becoming SQL data.\n'
[[ $(printf '%s\n' "$warningOutput" | grep -c '^WARNING: option --ssl-verify-server-cert') == 1 ]]
[[ "$warningOutput" == *'TLS settings are unchanged'* ]]
printf 'PASS: repeated TLS warning explained once without changing verification.\n'
publicOutput=$(MOCK_PUBLIC=test bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ "$publicOutput" == *'CLI and Qt validation passed'* && "$publicOutput" == *'PUBLIC grant rows: 2'* ]]
readerOutput=$(MOCK_ROLE=reader bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ "$readerOutput" == *'CLI and Qt validation passed'* && "$readerOutput" == *'including nested/activatable): 1'* ]]
printf 'PASS: unrelated PUBLIC test grants and a safe read role are accepted separately.\n'
expectPrivilegeRejection() {
   local variable=$1
   local value=$2
   rm -f -- "$temporaryDirectory/qt-ran"
   local rejectedOutput=''
   if rejectedOutput=$(env "$variable=$value" bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1); then
      printf 'FAIL: unsafe/incomplete privilege source accepted: %s=%s\n' "$variable" "$value" >&2; exit 1
   fi
   [[ ! -e "$temporaryDirectory/qt-ran" ]]
   [[ "$rejectedOutput" != *'FAKE-HASH'* && "$rejectedOutput" != *'fixture-password'* ]]
}
for scenario in target wildcard wildcardUnderscore global ddl grantOption unknown; do
   expectPrivilegeRejection MOCK_PUBLIC "$scenario"
done
for scenario in writer inactiveWriter nested unreadable publicRole; do
   expectPrivilegeRejection MOCK_ROLE "$scenario"
done
expectPrivilegeRejection MOCK_DIRECT_GLOBAL 1
expectPrivilegeRejection MOCK_DIRECT_WRITE 1
printf 'PASS: direct, role, nested/activatable role, wildcard/PUBLIC writes and incomplete audits block Qt.\n'
multiOutput=$(MOCK_ROLE=multi bash "$scriptPath" test --client "$clientPath" --build-dir "$temporaryDirectory/build" 2>&1)
[[ "$multiOutput" == *'CLI and Qt validation passed'* && "$multiOutput" == *'including nested/activatable): 2'* ]]
(
   readonly DATABASE_NAME=taskcompass_dev
   source "$repositoryDirectory/scripts/validate-grants.sh"
   for pattern in 'taskcompass\_dev' 'taskcompass_dev' 'taskcompass%' '%' 'TASKCOMPASS\_DEV'; do
      scopeMatchesDevelopmentDatabase "$pattern" || exit 1
   done
   for pattern in 'test' 'test_%' 'test\_%' 'taskcompass\_dev2' 'taskcompass\%dev'; do
      if scopeMatchesDevelopmentDatabase "$pattern"; then exit 1; fi
   done
)
printf 'PASS: multi-role parsing and escaped/wildcard/case-safe schema matching.\n'
bash -ec 'source "$1" --no-password; [[ "$TASKCOMPASS_DB_NAME" == taskcompass_dev ]]; clearTaskCompassDatabasePassword; [[ ! -v TASKCOMPASS_DB_PASSWORD ]]' bash "$repositoryDirectory/scripts/dev-environment.sh" >/dev/null
if bash "$repositoryDirectory/scripts/dev-environment.sh" >/dev/null 2>&1; then
   printf 'FAIL: executing environment helper should require sourcing.\n' >&2; exit 1
fi
printf 'PASS: environment sourcing and credential cleanup.\n'
