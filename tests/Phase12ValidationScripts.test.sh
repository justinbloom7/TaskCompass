#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fake clients/executables only; never connects to a database.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
helperDirectory="$repositoryDirectory/scripts/phase12"
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
export MOCK_DIRECTORY="$temporaryDirectory"
export MOCK_PASSWORD="fixture-test-password-quote'\\&"
unset MSYSTEM TASKCOMPASS_PHASE12_TEST_DB_HOST TASKCOMPASS_PHASE12_TEST_DB_PORT TASKCOMPASS_PHASE12_TEST_DB_NAME TASKCOMPASS_PHASE12_TEST_DB_USER TASKCOMPASS_PHASE12_TEST_DB_ISOLATED TASKCOMPASS_PHASE12_TEST_DB_PASSWORD
mkdir -p "$temporaryDirectory/build with spaces"
client="$temporaryDirectory/mariadb"
cat > "$client" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
[[ "$1" == --no-defaults ]]
for argument in "$@"; do [[ "$argument" != *fixture* ]] || exit 90; done
sql=$(cat)
printf 'query\n' >> "$MOCK_DIRECTORY/calls"
[[ "$sql" != *taskcompass_dev* && "$sql" != *taskcompass_integration* ]] || exit 91
case "$sql" in
   'SELECT @@hostname,VERSION(),@@port;' ) printf 'fake-server\t13.0.2-MariaDB\t3306\r\n' ;;
   'SELECT VERSION();') printf '13.0.2-MariaDB\n' ;;
   'SHOW GRANTS FOR PUBLIC;')
      if [[ "${MOCK_PUBLIC_UNSAFE:-0}" == 1 ]]; then printf 'GRANT INSERT ON `taskcompass%%`.* TO PUBLIC\n'
      else printf 'GRANT ALL PRIVILEGES ON `test\\_%%`.* TO PUBLIC\n'; fi ;;
   *'SELECT @@port,'*) printf '3306\t%s\t0\r\n' "${MOCK_EXISTING:-0}" ;;
   *'DEFAULT_CHARACTER_SET_NAME'*) printf '1\t%s\t0\t0\t0\r\n' "${MOCK_NONEMPTY:-0}" ;;
   'SELECT Host FROM mysql.user'*) printf '127.0.0.1\r\n' ;;
   *"CONCAT(TABLE_SCHEMA,'|'"*)
      for privilege in SELECT INSERT UPDATE CREATE DROP ALTER; do printf 'taskcompass\\_phase12\\_test|%s|NO\r\n' "$privilege"; done ;;
   'SELECT COUNT(*)'*) printf '%s\n' "${MOCK_EXTRA_GRANTS:-0}" ;;
   *'CREATE DATABASE taskcompass_phase12_test'*)
      [[ "$sql" == *"IDENTIFIED BY '${MOCK_PASSWORD//\'/\'\'}';"* ]]
      [[ "$sql" == *'ON `taskcompass\_phase12\_test`.*'* ]]
      [[ "$sql" != *'DROP TABLE'* && "$sql" != *'{{TEST_PASSWORD_SQL_LITERAL}}'* ]]
      printf 'create\n' >> "$MOCK_DIRECTORY/calls" ;;
   *) exit 92 ;;
esac
if [[ "${MOCK_FAIL:-0}" == 1 ]]; then printf 'connection failed: %s\n' "$MYSQL_PWD" >&2; exit 1; fi
CLIENT
chmod +x "$client"
cat > "$temporaryDirectory/build with spaces/OrganizationIntegrationTests" <<'EXECUTABLE'
#!/usr/bin/env bash
set -euo pipefail
[[ "$*" == '--run --allow-reset' ]]
[[ "$TASKCOMPASS_PHASE12_TEST_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_PHASE12_TEST_DB_PORT" == 3306 ]]
[[ "$TASKCOMPASS_PHASE12_TEST_DB_NAME" == taskcompass_phase12_test && "$TASKCOMPASS_PHASE12_TEST_DB_USER" == taskcompass_phase12_test ]]
[[ "$TASKCOMPASS_PHASE12_TEST_DB_ISOLATED" == 1 && "$TASKCOMPASS_PHASE12_TEST_DB_PASSWORD" == "$MOCK_PASSWORD" ]]
printf 'test\n' >> "$MOCK_DIRECTORY/calls"
exit "${MOCK_RESULT:-0}"
EXECUTABLE
chmod +x "$temporaryDirectory/build with spaces/OrganizationIntegrationTests"
for stage in 02-review-plan 03-setup-test 04-integration-test; do
   if [[ "$stage" == 02* ]]; then bash "$helperDirectory/$stage.sh" >/dev/null
   else bash "$helperDirectory/$stage.sh" --print-plan >/dev/null; bash "$helperDirectory/$stage.sh" >/dev/null; fi
done
if bash "$helperDirectory/03-setup-test.sh" --apply >/dev/null 2>&1; then exit 1; fi
if bash "$helperDirectory/04-integration-test.sh" --client "$client" >/dev/null 2>&1; then exit 1; fi
[[ ! -e "$temporaryDirectory/calls" ]]
printf 'PASS: previews/defaults and missing approval cannot contact databases.\n'
result=$(printf '%s\n' 'CREATE taskcompass_phase12_test' 'admin-fixture' "$MOCK_PASSWORD" "$MOCK_PASSWORD" |
   bash "$helperDirectory/03-setup-test.sh" --apply --approved --client "$client" 2>&1)
[[ $(cat "$temporaryDirectory/calls") == $'query\nquery\nquery\ncreate' && "$result" != *"$MOCK_PASSWORD"* ]]
export MOCK_EXISTING=1
if printf '%s\n' 'CREATE taskcompass_phase12_test' 'admin-fixture' | bash "$helperDirectory/03-setup-test.sh" --apply --approved --client "$client" >/dev/null 2>&1; then exit 1; fi
unset MOCK_EXISTING
printf 'PASS: fixed fresh target provisioning and existing-resource refusal.\n'
runTest() {
   printf '%s\n' 'TEST taskcompass_phase12_test WITH RUN-OWNED RESET' 'admin-fixture' "$MOCK_PASSWORD" |
      bash "$helperDirectory/04-integration-test.sh" --approved --client "$client" --build-dir "$temporaryDirectory/build with spaces"
}
: > "$temporaryDirectory/calls"
result=$(runTest)
[[ $(tail -1 "$temporaryDirectory/calls") == test && "$result" == *'exit code: 0'* ]]
for scenario in MOCK_NONEMPTY MOCK_EXTRA_GRANTS MOCK_PUBLIC_UNSAFE MOCK_FAIL; do
   : > "$temporaryDirectory/calls"
   export "$scenario=1"
   if result=$(runTest 2>&1); then exit 1; fi
   [[ $(cat "$temporaryDirectory/calls") != *test* && "$result" != *'admin-fixture'* && "$result" != *"$MOCK_PASSWORD"* ]]
   unset "$scenario"
done
: > "$temporaryDirectory/calls"
if TASKCOMPASS_PHASE12_TEST_DB_NAME=taskcompass_dev runTest >/dev/null 2>&1; then exit 1; fi
[[ ! -s "$temporaryDirectory/calls" ]]
for code in 1 77; do
   export MOCK_RESULT=$code
   set +e
   runTest >/dev/null
   result=$?
   set -e
   [[ "$result" == "$code" ]]
done
printf 'PASS: preflight blocks nonempty target/extra grants/PUBLIC writes/failure; unrelated PUBLIC test grants accepted; secret redaction and exit status preserved.\n'
