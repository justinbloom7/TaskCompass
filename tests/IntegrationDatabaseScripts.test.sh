#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Mock executables only: no live client, database, or credentials.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
helperDirectory="$repositoryDirectory/temp/testdb"
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
export MOCK_STATE_DIRECTORY="$temporaryDirectory"
export MOCK_PASSWORD='fixture-password-quote'\''&\backslash'
unset TASKCOMPASS_TEST_DB_HOST TASKCOMPASS_TEST_DB_PORT TASKCOMPASS_TEST_DB_NAME TASKCOMPASS_TEST_DB_USER TASKCOMPASS_TEST_DB_ISOLATED TASKCOMPASS_TEST_DB_PASSWORD
unset MSYSTEM
mkdir -p "$temporaryDirectory/client with spaces" "$temporaryDirectory/build with spaces"
client="$temporaryDirectory/client with spaces/mariadb"
cat > "$client" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
[[ "$1" == --no-defaults ]]
for argument in "$@"; do
   [[ "$argument" != *'fixture-password'* && "$argument" != *'admin-fixture'* ]] || exit 90
done
sql=$(cat)
if [[ "$sql" == SELECT* ]]; then
   printf 'probe\n' >> "$MOCK_STATE_DIRECTORY/calls"
   if [[ "${MOCK_PROBE_FAIL:-0}" == 1 ]]; then
      printf 'admin failure %s\n' "$MYSQL_PWD" >&2
      exit 1
   fi
   printf 'WARNING: mock client diagnostic on stderr\n' >&2
   if [[ "${MOCK_EXISTING:-0}" == 1 ]]; then
      printf '3306\t1\t0\r\n'
   else
      printf '3306\t0\t0\r\n'
   fi
else
   printf 'create\n' >> "$MOCK_STATE_DIRECTORY/calls"
   [[ "$sql" == *"SET SESSION sql_mode='NO_BACKSLASH_ESCAPES';"* ]]
   [[ "$sql" == *"IDENTIFIED BY '${MOCK_PASSWORD//\'/\'\'}';"* ]]
   [[ "$sql" == *'GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER'* ]]
   [[ "$sql" == *'ON `taskcompass\_integration`.*'* ]]
   [[ "$sql" != *'{{TEST_PASSWORD_SQL_LITERAL}}'* ]]
   [[ "$sql" != *'taskcompass_dev'* && "$sql" != *'DROP TABLE'* ]]
   if [[ "${MOCK_CREATE_FAIL:-0}" == 1 ]]; then
      printf 'failed SQL: %s\n' "$sql" >&2
      exit 1
   fi
fi
CLIENT
chmod +x "$client"
cat > "$temporaryDirectory/build with spaces/MariaDbIntegrationTests" <<'EXECUTABLE'
#!/usr/bin/env bash
set -euo pipefail
[[ "$*" == '--run --allow-reset' ]]
[[ "$TASKCOMPASS_TEST_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_TEST_DB_PORT" == 3306 ]]
[[ "$TASKCOMPASS_TEST_DB_NAME" == taskcompass_integration && "$TASKCOMPASS_TEST_DB_USER" == taskcompass_test ]]
[[ "$TASKCOMPASS_TEST_DB_ISOLATED" == 1 && "$TASKCOMPASS_TEST_DB_PASSWORD" == "$MOCK_PASSWORD" ]]
printf 'test\n' >> "$MOCK_STATE_DIRECTORY/calls"
exit "${MOCK_TEST_RESULT:-0}"
EXECUTABLE
chmod +x "$temporaryDirectory/build with spaces/MariaDbIntegrationTests"

bash "$helperDirectory/setup.sh" >/dev/null
plan=$(bash "$helperDirectory/setup.sh" --print-plan)
[[ "$plan" == *'{{TEST_PASSWORD_SQL_LITERAL}}'* && ! -e "$temporaryDirectory/calls" ]]
bash "$helperDirectory/test.sh" >/dev/null
if bash "$helperDirectory/test.sh" --run >/dev/null 2>&1; then exit 1; fi
[[ ! -e "$temporaryDirectory/calls" ]]
printf 'PASS: defaults/plan/missing test opt-in cannot contact a client or run tests.\n'

provisionInput() {
   printf '%s\n' 'CREATE taskcompass_integration' 'admin-fixture' "$MOCK_PASSWORD" "$MOCK_PASSWORD"
}
result=$(provisionInput | bash "$helperDirectory/setup.sh" --apply --client "$client" 2>&1)
[[ $(cat "$temporaryDirectory/calls") == $'probe\ncreate' ]]
[[ "$result" != *"$MOCK_PASSWORD"* && "$result" != *'admin-fixture'* && "$result" == *'Created the isolated'* ]]
printf 'PASS: fresh provisioning renders SQL in memory, escapes quote/backslash/ampersand and keeps passwords out of arguments/output.\n'

for scenario in existing probeFailure createFailure; do
   : > "$temporaryDirectory/calls"
   export MOCK_EXISTING=0 MOCK_PROBE_FAIL=0 MOCK_CREATE_FAIL=0
   case "$scenario" in
      existing) export MOCK_EXISTING=1 ;;
      probeFailure) export MOCK_PROBE_FAIL=1 ;;
      createFailure) export MOCK_CREATE_FAIL=1 ;;
   esac
   if result=$(provisionInput | bash "$helperDirectory/setup.sh" --apply --client "$client" 2>&1); then
      exit 1
   fi
   [[ "$result" != *"$MOCK_PASSWORD"* && "$result" != *"${MOCK_PASSWORD//\'/\'\'}"* && "$result" != *'admin-fixture'* ]]
   if [[ "$scenario" != createFailure ]]; then
      [[ $(cat "$temporaryDirectory/calls") == probe ]]
   else
      [[ $(cat "$temporaryDirectory/calls") == $'probe\ncreate' ]]
   fi
done
unset MOCK_EXISTING MOCK_PROBE_FAIL MOCK_CREATE_FAIL
for scenario in short mismatch; do
   : > "$temporaryDirectory/calls"
   first=$MOCK_PASSWORD
   second=$MOCK_PASSWORD
   if [[ "$scenario" == short ]]; then first=short; fi
   if [[ "$scenario" == mismatch ]]; then second=another-fixture-password; fi
   if result=$(printf '%s\n' 'CREATE taskcompass_integration' 'admin-fixture' "$first" "$second" | bash "$helperDirectory/setup.sh" --apply --client "$client" 2>&1); then exit 1; fi
   [[ $(cat "$temporaryDirectory/calls") == probe ]]
   [[ "$result" != *"$MOCK_PASSWORD"* && "$result" != *'admin-fixture'* ]]
done
: > "$temporaryDirectory/calls"
if printf 'cancel\n' | bash "$helperDirectory/setup.sh" --apply --client "$client" >/dev/null 2>&1; then exit 1; fi
[[ ! -s "$temporaryDirectory/calls" ]]
printf 'PASS: existing resources, failed inspection, failed creation and cancelled confirmation never overwrite or retry; diagnostics redact passwords.\n'

result=$(printf '%s\n' "$MOCK_PASSWORD" | bash "$helperDirectory/test.sh" --run --allow-reset --build-dir "$temporaryDirectory/build with spaces")
[[ $(cat "$temporaryDirectory/calls") == test && "$result" == *'exit code: 0'* ]]
: > "$temporaryDirectory/calls"
if TASKCOMPASS_TEST_DB_NAME=taskcompass_dev bash "$helperDirectory/test.sh" --run --allow-reset --build-dir "$temporaryDirectory/build with spaces" >/dev/null 2>&1; then exit 1; fi
[[ ! -s "$temporaryDirectory/calls" ]]
export MOCK_TEST_RESULT=1
if printf '%s\n' "$MOCK_PASSWORD" | bash "$helperDirectory/test.sh" --run --allow-reset --build-dir "$temporaryDirectory/build with spaces" >/dev/null; then exit 1; fi
export MOCK_TEST_RESULT=77
set +e
printf '%s\n' "$MOCK_PASSWORD" | bash "$helperDirectory/test.sh" --run --allow-reset --build-dir "$temporaryDirectory/build with spaces" >/dev/null
result=$?
set -e
[[ "$result" == 77 ]]
unset MOCK_TEST_RESULT
printf 'PASS: runner fixes the isolated target, rejects conflicting settings and propagates success/failure/skip status with paths containing spaces.\n'

(
   source "$helperDirectory/env.sh" --no-password
   [[ "$TASKCOMPASS_TEST_DB_NAME" == taskcompass_integration && "$TASKCOMPASS_TEST_DB_USER" == taskcompass_test ]]
   TASKCOMPASS_TEST_DB_PASSWORD=$MOCK_PASSWORD
   clearTaskCompassTestPassword
   [[ ! -v TASKCOMPASS_TEST_DB_PASSWORD ]]
   if source "$helperDirectory/env.sh" --password </dev/null; then exit 1; fi
   [[ ! -v TASKCOMPASS_TEST_DB_PASSWORD ]]
)
if bash "$helperDirectory/env.sh" >/dev/null 2>&1; then exit 1; fi
printf 'PASS: source-only environment and explicit parent-shell credential cleanup.\n'
