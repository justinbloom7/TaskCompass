#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fixture clients only, including a streaming admin client; no real server.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_OPERATOR_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12RestoreScripts.test.sh"
helperDirectory="$repositoryDirectory/scripts/phase12/b74a"
export MOCK_REPOSITORY="$repositoryDirectory"
trap 'printf "FAIL: B7.4a offline test line %s.\n" "$LINENO" >&2' ERR
cat > "$temporaryDirectory/bin/mariadb" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
if [[ " $* " == *' --version '* ]]; then printf 'fixture\n'; exit 0; fi
[[ " $* " == *' --database=taskcompass_phase12_test '* && " $* " != *' --force '* ]]
for arg in "$@"; do [[ "$arg" != *"$MOCK_SECRET"* ]] || exit 90; done
if [[ " $* " == *' --user=root '* ]]; then
   [[ " $* " == *' --unbuffered '* && " $* " == *' --skip-reconnect '* && ! -v MYSQL_PWD ]]
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
      [[ " $* " == *' --protocol=TCP '* && " $* " == *' --password '* ]]
   else
      [[ " $* " == *' --protocol=SOCKET '* && " $* " != *' --password '* ]]
   fi
   buffer=''
   while IFS= read -r line; do
      if [[ "$line" != "SELECT '__TASKCOMPASS_B74_END__';" ]]; then buffer+="$line"$'\n'; continue; fi
      if [[ "$buffer" == *"SELECT 'SERVER'"* ]]; then
         printf 'root-check\n' >> "$MOCK_DIRECTORY/calls"
         host=feledria; version=10.5.29-MariaDB; tableCase=0
         [[ "${MSYSTEM:-}" != UCRT64 ]] || { host=feldria-w10; version=13.0.2-MariaDB; tableCase=1; }
         [[ "${MOCK_WRONG_SERVER:-0}" != 1 ]] || host=wrong-host
         printf 'SERVER\t%s\t%s\t3306\ttaskcompass_phase12_test\troot@localhost\t%s\t1\t1\n' "$host" "$version" "$tableCase"
         printf 'STATE\t1\t0\t0\t0\t%s\nHOST\t127.0.0.1\nGLOBAL\t0\nCOLUMN\t0\nROLE\t0\n' "${MOCK_OTHER_SESSION:-0}"
         for name in commandrecord dataset localchange schemamigration task; do printf 'OBJECT\t%s\tBASE TABLE\tInnoDB\n' "$name"; done
         if [[ "$(cat "$MOCK_SCHEMA")" == 2 ]]; then
            for name in migrationattempt pillar project objective; do printf 'OBJECT\t%s\tBASE TABLE\tInnoDB\n' "$name"; done
         fi
         if [[ "$(cat "$MOCK_ROLE")" == broad ]]; then
            for right in SELECT INSERT UPDATE CREATE DROP ALTER; do printf 'RIGHT\ttaskcompass\\_phase12\\_test\t%s\tNO\n' "$right"; done
         else
            printf 'RIGHT\ttaskcompass\\_phase12\\_test\tSELECT\tNO\n'
            for name in pillar project objective; do printf 'TABLE_RIGHT\ttaskcompass_phase12_test\t%s\tCREATE\tNO\n' "$name"; done
            for right in CREATE INSERT UPDATE; do printf 'TABLE_RIGHT\ttaskcompass_phase12_test\tmigrationattempt\t%s\tNO\n' "$right"; done
            for name in task commandrecord localchange; do printf 'TABLE_RIGHT\ttaskcompass_phase12_test\t%s\tALTER\tNO\n' "$name"; done
            printf 'TABLE_RIGHT\ttaskcompass_phase12_test\tschemamigration\tINSERT\tNO\n'
            [[ "$(cat "$MOCK_ROLE")" != extra ]] || printf 'TABLE_RIGHT\ttaskcompass_phase12_test\ttask\tUPDATE\tNO\n'
         fi
      elif [[ "$buffer" == *'REVOKE SELECT, INSERT, UPDATE, CREATE, DROP, ALTER'* ]]; then
         [[ "$buffer" != *taskcompass_dev* && "$buffer" != *'CREATE USER'* ]]
         printf 'grant\n' >> "$MOCK_DIRECTORY/calls"
         printf 'narrow\n' > "$MOCK_ROLE"
         [[ "${MOCK_GRANT_FAIL:-0}" != 1 ]] || exit 1
         [[ "${MOCK_EXTRA_GRANT:-0}" != 1 ]] || printf 'extra\n' > "$MOCK_ROLE"
      elif [[ "$buffer" == *'SHOW GRANTS FOR PUBLIC;'* ]]; then
         if [[ "${MOCK_PUBLIC_UNSAFE:-0}" == 1 ]]; then printf 'GRANT INSERT ON `taskcompass%%`.* TO PUBLIC\n'
         else printf 'GRANT ALL PRIVILEGES ON `test\\_%%`.* TO PUBLIC\n'; fi
      else exit 91; fi
      printf '__TASKCOMPASS_B74_END__\n'
      buffer=''
   done
   exit 0
fi
[[ " $* " == *' --user=taskcompass_phase12_test '* && "$MYSQL_PWD" == "$MOCK_SECRET" ]]
sql=$(cat)
case "$sql" in
   'SELECT DATABASE(),CURRENT_USER();') printf 'taskcompass_phase12_test\ttaskcompass_phase12_test@127.0.0.1\n' ;;
   *"'COUNT:SchemaMigration'"*)
      printf 'fixture-legacy\n'
      if [[ "${MOCK_LEGACY_DRIFT:-0}" == 1 || ( "${MOCK_POST_DRIFT:-0}" == 1 && "$(cat "$MOCK_SCHEMA")" == 2 ) ]]; then printf 'drift\n'; fi ;;
   *'SHOW CREATE TABLE MigrationAttempt;'*) printf 'fixture-DDL2\n' ;;
   *'SHOW CREATE TABLE SchemaMigration;'*) printf 'fixture-DDL\n' ;;
   *'oldTasksWithUnexpectedOwner'*)
      fp=$(printf '%064d' 0)
      one=$(sha256sum < "$MOCK_REPOSITORY/migrations/mariadb/001_PersistentTaskFoundation.sql"); one=${one%% *}
      two=$(sha256sum < "$MOCK_REPOSITORY/migrations/mariadb/002_OrganizationalManagement.sql"); two=${two%% *}
      printf 'Pillar\t%s\nProject\t0\nObjective\t0\n0\n0\n0\n' "${MOCK_ORGANIZATIONS:-0}"
      printf '1\t%s\t%s\t1000\n2\t%s\t%s\t1001\n2\t%s\t%s\t1000\t1001\t9\tcompleted\n' "$one" "$fp" "$two" "$fp" "$two" "$fp" ;;
   *'AS ruleName,COUNT(*) AS violations'*)
      pattern="^SELECT '([^']+)' AS ruleName,COUNT"
      while IFS= read -r rule; do
         if [[ "$rule" =~ $pattern ]]; then printf '%s\t%s\n' "${BASH_REMATCH[1]}" "${MOCK_HEALTH_FAIL:-0}"; fi
      done <<< "$sql" ;;
   *) exit 92 ;;
esac
CLIENT
cat > "$temporaryDirectory/build with spaces/TaskCompassMigrate" <<'MIGRATE'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$*" == '--print-migration 1' || "$*" == '--print-migration 2' ]]; then
   if [[ "${MOCK_BAD_EMBEDDED:-0}" == 1 ]]; then printf 'wrong embedded SQL\n'; exit 0; fi
   if [[ "$2" == 1 ]]; then cat "$MOCK_REPOSITORY/migrations/mariadb/001_PersistentTaskFoundation.sql"
   else cat "$MOCK_REPOSITORY/migrations/mariadb/002_OrganizationalManagement.sql"; fi
   exit 0
fi
[[ "$*" == '--verify --phase12-test --to-version 1' || "$*" == '--verify --phase12-test --to-version 2' || "$*" == '--apply --phase12-test --to-version 2' ]]
[[ "$TASKCOMPASS_PHASE12_TEST_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_PHASE12_TEST_DB_PORT" == 3306 && "$TASKCOMPASS_PHASE12_TEST_DB_NAME" == taskcompass_phase12_test && "$TASKCOMPASS_PHASE12_TEST_DB_USER" == taskcompass_phase12_test && "$TASKCOMPASS_PHASE12_TEST_DB_ISOLATED" == 1 && "$TASKCOMPASS_PHASE12_TEST_DB_PASSWORD" == "$MOCK_SECRET" ]]
if [[ "$1" == --apply ]]; then
   [[ "$(cat "$MOCK_ROLE")" == narrow && "$(cat "$MOCK_SCHEMA")" == 1 ]]
   printf 'apply\n' >> "$MOCK_DIRECTORY/calls"
   if [[ "${MOCK_APPLY_FAIL:-0}" == 1 ]]; then printf 'partial\n' > "$MOCK_SCHEMA"; exit 1; fi
   printf '2\n' > "$MOCK_SCHEMA"
fi
[[ "$(cat "$MOCK_SCHEMA")" == "$4" ]]
dataset=21be9da7-b767-4578-b972-cc9a35122ddb
[[ "${MSYSTEM:-}" != UCRT64 ]] || dataset=457bb52b-fea4-4347-9c47-675c6b035c90
printf 'Schema %s verified; dataset %s\n' "$4" "$dataset"
MIGRATE
setup() {
   fixture "$1"
   export MOCK_SCHEMA="$snapshot.schema" MOCK_ROLE="$snapshot.role"
   printf '1\n' > "$MOCK_SCHEMA"; printf 'broad\n' > "$MOCK_ROLE"
   digest=$(sha256sum < "$snapshot/after-gui.sql"); digest=${digest%% *}
   for name in b73-import-attempt.txt b73-import-success.txt; do printf '%s\n' "$digest" > "$snapshot/$name"; done
   cp "$snapshot/legacy-before.hex.tsv" "$snapshot/rehearsal1.hex.tsv"
   cp "$snapshot/schema1.ddl.txt" "$snapshot/rehearsal1.ddl.txt"
   (cd -- "$snapshot" && sha256sum rehearsal1.hex.tsv rehearsal1.ddl.txt b73-import-attempt.txt b73-import-success.txt > b73-comparison.sha256)
}
grantsInput() { printf 'NARROW GRANTS taskcompass_phase12_test\nWRITERS CLOSED; BASELINE UNCHANGED\n%s\n' "$MOCK_SECRET"; }
migrateInput() { printf 'MIGRATE REHEARSAL taskcompass_phase12_test TO 2\nWRITERS CLOSED; BASELINE UNCHANGED\n%s\n' "$MOCK_SECRET"; }
"$TASKCOMPASS_TEST_PYTHON" - "$helperDirectory/preflight.sql" <<'READONLY'
import pathlib, re, sys
source = re.sub(r'^--.*$', '', pathlib.Path(sys.argv[1]).read_text(), flags=re.M)
assert all(s.strip().startswith('SELECT ') for s in source.split(';') if s.strip())
READONLY
setup 'linux rehearsal'
for stage in 01-preflight 02-grants 03-migrate 04-preservation; do
   run "$stage" --help >/dev/null
   run "$stage" --print-plan >/dev/null
   if result=$(run "$stage" 2>&1); then exit 1; fi
done
[[ ! -e "$temporaryDirectory/calls" ]]
for failureMode in MOCK_BAD_EMBEDDED MOCK_LEGACY_DRIFT MOCK_WRONG_SERVER MOCK_OTHER_SESSION; do
   export "$failureMode=1"
   if result=$(printf '%s\n' "$MOCK_SECRET" | run 01-preflight --approved 2>&1); then exit 1; fi
   [[ ! -e "$snapshot/b74a-grants-attempt.txt" && "$(cat "$MOCK_ROLE")" == broad ]]
   unset "$failureMode"
done
result=$(printf '%s\n' "$MOCK_SECRET" | run 01-preflight --approved 2>&1)
[[ "$result" == *'exact broad grants'* ]]
result=$(grantsInput | run 02-grants --approved 2>&1)
[[ "$result" == *'PASS: exact narrow grant transition'* && "$result" != *"$MOCK_SECRET"* ]]
if result=$(grantsInput | run 02-grants --approved 2>&1); then exit 1; fi
result=$(migrateInput | run 03-migrate --approved 2>&1)
[[ "$result" == *'PASS: one-shot clone migration002'* ]]
if result=$(migrateInput | run 03-migrate --approved 2>&1); then exit 1; fi
result=$(printf '%s\n' "$MOCK_SECRET" | run 04-preservation --approved 2>&1)
[[ "$result" == *'PASS: B7.4a narrow-role schema2 clone'* ]]
printf 'PASS: fresh baseline/identity/embedded SQL/role guards; one-shot grant+migration and all preservation/health checks.\n'
setup 'partial grants'
export MOCK_GRANT_FAIL=1
if result=$(grantsInput | run 02-grants --approved 2>&1); then exit 1; fi
unset MOCK_GRANT_FAIL
[[ -s "$snapshot/b74a-grants-attempt.txt" && ! -e "$snapshot/b74a-grants-success.txt" ]]
if result=$(grantsInput | run 02-grants --approved 2>&1); then exit 1; fi
if result=$(migrateInput | run 03-migrate --approved 2>&1); then exit 1; fi
setup 'extra grants'
export MOCK_EXTRA_GRANT=1
if result=$(grantsInput | run 02-grants --approved 2>&1); then exit 1; fi
unset MOCK_EXTRA_GRANT
[[ ! -e "$snapshot/b74a-grants-success.txt" && ! -e "$snapshot/b74a-migration-attempt.txt" ]]
setup 'drift before migration'
result=$(grantsInput | run 02-grants --approved 2>&1)
export MOCK_LEGACY_DRIFT=1
if result=$(migrateInput | run 03-migrate --approved 2>&1); then exit 1; fi
unset MOCK_LEGACY_DRIFT
[[ ! -e "$snapshot/b74a-migration-attempt.txt" && "$(cat "$MOCK_SCHEMA")" == 1 ]]
setup 'partial migration'
result=$(grantsInput | run 02-grants --approved 2>&1)
export MOCK_APPLY_FAIL=1
if result=$(migrateInput | run 03-migrate --approved 2>&1); then exit 1; fi
unset MOCK_APPLY_FAIL
[[ -s "$snapshot/b74a-migration-attempt.txt" && ! -e "$snapshot/b74a-migration-success.txt" && "$(cat "$MOCK_SCHEMA")" == partial ]]
if result=$(migrateInput | run 03-migrate --approved 2>&1); then exit 1; fi
printf 'PASS: failed/extra/partial grants and migration retain markers/state, cannot retry/resume/broaden.\n'
export MSYSTEM=UCRT64
setup 'windows rehearsal'
result=$(grantsInput | run 02-grants --approved 2>&1)
result=$(migrateInput | run 03-migrate --approved 2>&1)
result=$(printf '%s\n' "$MOCK_SECRET" | run 04-preservation --approved 2>&1)
[[ "$result" == *'PASS: B7.4a narrow-role schema2 clone'* ]]
for failureMode in MOCK_POST_DRIFT MOCK_HEALTH_FAIL MOCK_ORGANIZATIONS; do
   setup "windows $failureMode"
   result=$(grantsInput | run 02-grants --approved 2>&1)
   result=$(migrateInput | run 03-migrate --approved 2>&1)
   export "$failureMode=1"
   if result=$(printf '%s\n' "$MOCK_SECRET" | run 04-preservation --approved 2>&1); then exit 1; fi
   unset "$failureMode"
   [[ ! -e "$snapshot/b74a-preservation.sha256" ]]
done
printf 'PASS: mocked Windows admin streaming session; altered legacy data, nonzero health/new organizations cannot seal.\n'
