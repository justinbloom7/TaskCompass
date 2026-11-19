#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Isolated fake clients and private fixture files only; never real SQL.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_B75_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12RuntimeRehearsalScripts.test.sh"
trap 'code=$?; printf "Offline fixture failure in %s at line %s (exit %s).\n" "${BASH_SOURCE[0]}" "$LINENO" "$code" >&2; if [[ -n "${result:-}" ]]; then printf "%s\n" "${result//${MOCK_SECRET:-unused}/[fixture-secret]}" >&2; fi' ERR
trap 'printf "FAIL: B7.5 offline test line %s.\n" "$LINENO" >&2; if [[ -n "${result:-}" ]]; then printf "%s\n" "${result//${MOCK_SECRET:-unused}/[fixture-secret]}" >&2; fi' ERR
cp "$temporaryDirectory/bin/mariadb" "$temporaryDirectory/bin/clone-client"
cp "$temporaryDirectory/build with spaces/TaskCompassMigrate" "$temporaryDirectory/build with spaces/CloneMigrate"
cat > "$temporaryDirectory/bin/mariadb" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
if [[ " $* " == *' --version '* || " $* " == *' --database=taskcompass_phase12_test '* ]]; then exec "$MOCK_DIRECTORY/bin/clone-client" "$@"; fi
[[ " $* " == *' --database=taskcompass_dev '* && " $* " == *' --skip-reconnect '* && " $* " != *' --force '* ]]
for arg in "$@"; do [[ "$arg" != *"$MOCK_SECRET"* ]] || exit 90; done
printf 'dev-client\n' >> "$MOCK_DIRECTORY/calls"
if [[ " $* " == *' --user=root '* ]]; then
   [[ ! -v MYSQL_PWD && " $* " == *' --unbuffered '* ]]
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then [[ " $* " == *' --password '* && " $* " == *' --protocol=TCP '* ]]
   else [[ " $* " == *' --protocol=SOCKET '* ]]; fi
   buffer=''
   while IFS= read -r line; do
      if [[ "$line" != "SELECT '__TASKCOMPASS_B74_END__';" ]]; then buffer+="$line"$'\n'; continue; fi
      if [[ "$buffer" == *"SELECT 'SERVER'"* ]]; then
         host=feledria; version=10.5.29-MariaDB; tableCase=0
         [[ "${MSYSTEM:-}" != UCRT64 ]] || { host=feldria-w10; version=13.0.2-MariaDB; tableCase=1; }
         [[ "${DEV_WRONG_SERVER:-0}" != 1 ]] || host=wrong
         printf 'SERVER\t%s\t%s\t3306\ttaskcompass_dev\troot@localhost\t%s\t1\t1\n' "$host" "$version" "$tableCase"
         printf 'STATE\t1\t0\t0\t0\t%s\n' "${DEV_ACTIVE_SESSION:-0}"
         for name in commandrecord dataset localchange schemamigration task; do printf 'OBJECT\t%s\tBASE TABLE\tInnoDB\n' "$name"; done
         if [[ "$(cat "$DEV_SCHEMA")" == 2 ]]; then
            for name in migrationattempt pillar project objective; do printf 'OBJECT\t%s\tBASE TABLE\tInnoDB\n' "$name"; done
         fi
         for account in taskcompass_dev taskcompass_migrate taskcompass_app; do
            printf 'HOST\t%s\t127.0.0.1\nGLOBAL\t%s\t0\nCOLUMN\t%s\t0\nROLE\t%s\t0\n' "$account" "$account" "$account" "$account"
            if [[ "$account" != taskcompass_app ]]; then printf 'RIGHT\t%s\ttaskcompass\\_dev\tSELECT\tNO\n' "$account"; fi
            if [[ "$account" == taskcompass_app ]]; then
               for right in SELECT INSERT UPDATE; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\ttask\t%s\tNO\n' "$account" "$right"; done
               for name in commandrecord localchange; do for right in SELECT INSERT; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\t%s\t%s\tNO\n' "$account" "$name" "$right"; done; done
               for name in dataset schemamigration; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\t%s\tSELECT\tNO\n' "$account" "$name"; done
               if [[ "$(cat "$DEV_POLICY")" == final ]]; then
                  for name in pillar project objective; do for right in SELECT INSERT UPDATE; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\t%s\t%s\tNO\n' "$account" "$name" "$right"; done; done
                  printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\tmigrationattempt\tSELECT\tNO\n' "$account"
               fi
            elif [[ "$account" == taskcompass_migrate && "$(cat "$DEV_POLICY")" == migration ]]; then
               for name in pillar project objective; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\t%s\tCREATE\tNO\n' "$account" "$name"; done
               for right in CREATE INSERT UPDATE; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\tmigrationattempt\t%s\tNO\n' "$account" "$right"; done
               for name in task commandrecord localchange; do printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\t%s\tALTER\tNO\n' "$account" "$name"; done
               printf 'TABLE_RIGHT\t%s\ttaskcompass_dev\tschemamigration\tINSERT\tNO\n' "$account"
            fi
         done
         [[ "${DEV_EXTRA_GRANT:-0}" != 1 ]] || printf 'RIGHT\ttaskcompass_app\tother_database\tSELECT\tNO\n'
      elif [[ "$buffer" == *'GRANT CREATE ON taskcompass_dev.Pillar'* ]]; then
         printf 'dev-grants\n' >> "$MOCK_DIRECTORY/calls"
         printf 'migration\n' > "$DEV_POLICY"
         [[ "${DEV_GRANT_FAIL:-0}" != 1 ]] || exit 1
         [[ "${DEV_GRANT_INTERRUPT:-0}" != 1 ]] || { kill -TERM "$PPID"; exit 1; }
      elif [[ "$buffer" == *'REVOKE CREATE ON taskcompass_dev.Pillar'* ]]; then
         printf 'dev-runtime\n' >> "$MOCK_DIRECTORY/calls"
         printf 'final\n' > "$DEV_POLICY"
         [[ "${DEV_RUNTIME_FAIL:-0}" != 1 ]] || exit 1
      elif [[ "$buffer" == *'SHOW GRANTS FOR PUBLIC;'* ]]; then
         if [[ "${DEV_PUBLIC_UNSAFE:-0}" == 1 ]]; then printf 'GRANT INSERT ON `taskcompass%%`.* TO PUBLIC\n'
         else printf 'GRANT ALL PRIVILEGES ON `test\\_%%`.* TO PUBLIC\n'; fi
      else exit 91; fi
      printf '__TASKCOMPASS_B74_END__\n'; buffer=''
   done
   exit 0
fi
user=''
for arg in "$@"; do case "$arg" in --user=*) user=${arg#--user=} ;; esac; done
case "$user" in taskcompass_dev|taskcompass_migrate|taskcompass_app) ;; *) exit 90 ;; esac
[[ "$MYSQL_PWD" == "$MOCK_SECRET" ]]
sql=$(cat)
case "$sql" in
 'SELECT DATABASE(),CURRENT_USER();') printf 'taskcompass_dev\t%s@127.0.0.1\n' "$user" ;;
 *"'COUNT:MigrationAttempt'"*) printf 'fixture-dev-nine\n'; [[ "${DEV_FULL_DRIFT:-0}" != 1 ]] || printf 'drift\n' ;;
 *"'COUNT:SchemaMigration'"*) printf 'fixture-legacy\n'; [[ "${DEV_LEGACY_DRIFT:-0}" != 1 ]] || printf 'drift\n' ;;
 *'SHOW CREATE TABLE MigrationAttempt;'*)
   printf 'fixture-DDL2%s\nLocalChange\tCREATE TABLE `LocalChange` (\n' "${DEV_DDL_DRIFT:+-different}"
   index=changeTask; [[ "${DEV_TASK_INDEX_V2:-0}" != 1 ]] || index=changeTaskV2
   printf '  KEY `%s` (`datasetId`,`taskId`),\n) ENGINE=InnoDB\n' "$index" ;;
 *'SHOW CREATE TABLE SchemaMigration;'*) printf 'fixture-DDL\n' ;;
 *'oldTasksWithUnexpectedOwner'*)
   if [[ "${DEV_BAD_JOURNAL:-0}" == 1 ]]; then printf 'bad journal\n'; exit 0; fi
   MYSQL_PWD="$MOCK_SECRET" "$MOCK_DIRECTORY/bin/clone-client" --no-defaults --skip-reconnect --database=taskcompass_phase12_test --user=taskcompass_phase12_test <<< "$sql" ;;
 *'AS ruleName,COUNT(*) AS violations'*)
   MOCK_HEALTH_FAIL="${DEV_BAD_HEALTH:-0}" MYSQL_PWD="$MOCK_SECRET" "$MOCK_DIRECTORY/bin/clone-client" --no-defaults --skip-reconnect --database=taskcompass_phase12_test --user=taskcompass_phase12_test <<< "$sql" ;;
 *) exit 92 ;;
esac
CLIENT
cat > "$temporaryDirectory/build with spaces/TaskCompassMigrate" <<'MIGRATE'
#!/usr/bin/env bash
set -euo pipefail
if [[ "$*" == --print-migration* || "$*" == *--phase12-test* ]]; then exec "$MOCK_DIRECTORY/build with spaces/CloneMigrate" "$@"; fi
[[ "$TASKCOMPASS_DB_NAME" == taskcompass_dev && "$TASKCOMPASS_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_DB_PORT" == 3306 && "$TASKCOMPASS_DB_ISOLATED" == 1 ]]
if [[ "$1" == --apply && "$TASKCOMPASS_DB_USER" == taskcompass_app ]]; then
 printf 'Migration --apply requires the dedicated migration/test account, never runtime/admin credentials.\n' >&2; exit 1
fi
[[ "${MSYSTEM:-}" != UCRT64 || "${MSYS2_ENV_CONV_EXCL:-}" == *TASKCOMPASS_DB_PASSWORD* ]]
[[ "$TASKCOMPASS_DB_PASSWORD" == "$MOCK_SECRET" ]]
case "$TASKCOMPASS_DB_USER" in taskcompass_migrate) ;; taskcompass_app) [[ "$(cat "$DEV_POLICY")" == final ]] ;; *) exit 91 ;; esac
if [[ "$1" == --apply ]]; then
 [[ "$*" == '--apply --to-version 2' && "$TASKCOMPASS_DB_USER" == taskcompass_migrate && "$(cat "$DEV_SCHEMA")" == 1 && "$(cat "$DEV_POLICY")" == migration ]]
 printf 'dev-apply\n' >> "$MOCK_DIRECTORY/calls"
 if [[ "${DEV_APPLY_FAIL:-0}" == 1 ]]; then printf 'partial\n' > "$DEV_SCHEMA"; exit 1; fi
 if [[ "${DEV_APPLY_INTERRUPT:-0}" == 1 ]]; then printf 'partial\n' > "$DEV_SCHEMA"; kill -TERM "$PPID"; exit 1; fi
 printf '2\n' > "$DEV_SCHEMA"
else [[ "$1" == --verify && "$(cat "$DEV_SCHEMA")" == "$3" ]]; fi
dataset=21be9da7-b767-4578-b972-cc9a35122ddb
[[ "${MSYSTEM:-}" != UCRT64 ]] || dataset=457bb52b-fea4-4347-9c47-675c6b035c90
[[ "${DEV_WRONG_UUID:-0}" != 1 ]] || dataset=bad-uuid
printf 'Schema %s verified; dataset %s\n' "$(cat "$DEV_SCHEMA")" "$dataset"
MIGRATE
setup75() {
   prepare "$1"
   result=$(runtimeInput | run 02-runtime-grants --approved 2>&1)
   result=$(probeInput | run 03-probe --approved 2>&1)
   export DEV_SCHEMA="$snapshot.dev-schema" DEV_POLICY="$snapshot.dev-policy"
   printf '1\n' > "$DEV_SCHEMA"; printf 'original\n' > "$DEV_POLICY"
   helperDirectory="$repositoryDirectory/scripts/phase12/b75"
}
# Platform string is explicit, not inferred from approval on the other platform.
run75() { local stage=$1; shift; local selected=linux; [[ "${MSYSTEM:-}" != UCRT64 ]] || selected=windows; run "$stage" --platform "$selected" "$@"; }
passwords75() { printf '%s\n%s\n%s\n' "$MOCK_SECRET" "$MOCK_SECRET" "$MOCK_SECRET"; }
# Builtin-only count: no ripgrep/grep dependency, including the zero-match case.
countDevMutations() {
   local file=$1 line count=0
   while IFS= read -r line || [[ -n "$line" ]]; do
      case "$line" in dev-grants|dev-apply|dev-runtime) count=$((count+1)) ;; esac
   done < "$file"
   printf '%s\n' "$count"
}
if [[ "${TASKCOMPASS_B75_INSPECTION_FIXTURES_ONLY:-0}" == 1 ]]; then return 0; fi
for platform in linux windows; do
 export MSYSTEM=''; [[ "$platform" != windows ]] || export MSYSTEM=UCRT64
 setup75 "$platform dev"
 before=$(wc -l < "$MOCK_DIRECTORY/calls")
 for stage in 01-preflight 02-migration-grants 03-migrate 04-runtime-grants 05-verify; do
   run75 "$stage" --print-plan >/dev/null
   if result=$(run75 "$stage" 2>&1); then exit 1; fi
 done
 if result=$(run 01-preflight --approved --platform wrong 2>&1); then exit 1; fi
 [[ "$(wc -l < "$MOCK_DIRECTORY/calls")" == "$before" ]]
 if [[ "$platform" == windows ]]; then
   export DEV_PUBLIC_UNSAFE=1
   if result=$(passwords75 | run75 01-preflight --approved 2>&1); then exit 1; fi
   unset DEV_PUBLIC_UNSAFE
 fi
 result=$(passwords75 | run75 01-preflight --approved 2>&1)
 [[ "$result" == *'PASS: B7.5 fresh dev'* ]]
 for stage in 02-migration-grants 03-migrate 04-runtime-grants 05-verify; do
   result=$(passwords75 | run75 "$stage" --approved 2>&1)
   [[ "$result" != *"$MOCK_SECRET"* ]]
   if result=$(passwords75 | run75 "$stage" --approved 2>&1); then exit 1; fi
 done
 [[ -s "$snapshot/b75-final.sha256" && "$(cat "$DEV_SCHEMA")" == 2 && "$(cat "$DEV_POLICY")" == final ]]
 printf 'PASS: %s mock dev upgrade/final19 rights and original preservation, no typed confirmations/replay.\n' "$platform"
done
export MSYSTEM=''
setup75 'negative development checks'
cp "$snapshot/b74b-probe-result.tsv" "$temporaryDirectory/saved-probe.tsv"
printf 'tampered\n' >> "$snapshot/b74b-probe-result.tsv"
if result=$(passwords75 | run75 01-preflight --approved 2>&1); then exit 1; fi
cp "$temporaryDirectory/saved-probe.tsv" "$snapshot/b74b-probe-result.tsv"
[[ ! -e "$snapshot/b75-grants-attempt.txt" ]]
for mode in DEV_WRONG_SERVER DEV_ACTIVE_SESSION DEV_EXTRA_GRANT DEV_LEGACY_DRIFT DEV_WRONG_UUID; do
 export "$mode=1"
 if result=$(passwords75 | run75 02-migration-grants --approved 2>&1); then exit 1; fi
 unset "$mode"
 [[ ! -e "$snapshot/b75-grants-attempt.txt" && "$(cat "$DEV_POLICY")" == original ]]
done
for mode in DEV_GRANT_FAIL DEV_GRANT_INTERRUPT; do
 setup75 "$mode"
 export "$mode=1"
 if result=$(passwords75 | run75 02-migration-grants --approved 2>&1); then exit 1; fi
 unset "$mode"
 [[ -s "$snapshot/b75-grants-attempt.txt" && ! -e "$snapshot/b75-grants-success.txt" ]]
 if result=$(passwords75 | run75 02-migration-grants --approved 2>&1); then exit 1; fi
done
for mode in DEV_APPLY_FAIL DEV_APPLY_INTERRUPT DEV_BAD_JOURNAL DEV_BAD_HEALTH; do
 setup75 "$mode"
 result=$(passwords75 | run75 02-migration-grants --approved 2>&1)
 export "$mode=1"
 if result=$(passwords75 | run75 03-migrate --approved 2>&1); then exit 1; fi
 unset "$mode"
 [[ -s "$snapshot/b75-migration-attempt.txt" && ! -e "$snapshot/b75-migration-success.txt" ]]
 if result=$(passwords75 | run75 03-migrate --approved 2>&1); then exit 1; fi
done
setup75 'full schema drift after migration'
result=$(passwords75 | run75 02-migration-grants --approved 2>&1)
result=$(passwords75 | run75 03-migrate --approved 2>&1)
export DEV_FULL_DRIFT=1
if result=$(passwords75 | run75 04-runtime-grants --approved 2>&1); then exit 1; fi
unset DEV_FULL_DRIFT
[[ ! -e "$snapshot/b75-runtime-attempt.txt" && "$(cat "$DEV_POLICY")" == migration ]]
setup75 'runtime partial'
result=$(passwords75 | run75 02-migration-grants --approved 2>&1)
result=$(passwords75 | run75 03-migrate --approved 2>&1)
export DEV_RUNTIME_FAIL=1
if result=$(passwords75 | run75 04-runtime-grants --approved 2>&1); then exit 1; fi
unset DEV_RUNTIME_FAIL
[[ -s "$snapshot/b75-runtime-attempt.txt" && ! -e "$snapshot/b75-runtime-success.txt" ]]
if result=$(passwords75 | run75 04-runtime-grants --approved 2>&1); then exit 1; fi
printf 'PASS: sealed evidence/PUBLIC/wrong identity/grants/writers/drift rejected; interrupted/partial grant and DDL markers preserved without replay.\n'

"$TASKCOMPASS_TEST_PYTHON" - "$repositoryDirectory/scripts/phase12/b75/preflight.sql" <<'READONLY'
import pathlib, re, sys
source = re.sub(r'^--.*$', '', pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"), flags=re.M)
assert all(part.strip().startswith('SELECT ') for part in source.split(';') if part.strip())
assert "USER IN ('taskcompass_dev','taskcompass_migrate','taskcompass_app')" in source
READONLY
