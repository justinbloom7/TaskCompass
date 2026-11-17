#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline fake executables only. No MariaDB connection or real backup.
set -euo pipefail
umask 077
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
helperDirectory="$repositoryDirectory/scripts/phase12/b72"
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
trap 'printf "FAIL: B7.2 offline script tests at line %s.\n" "$LINENO" >&2' ERR
export MOCK_DIRECTORY="$temporaryDirectory"
export MOCK_SECRET='b72-fixture-secret'
originalStat=$(command -v stat)
unset MSYSTEM MYSQL_PWD TASKCOMPASS_DB_PASSWORD
mkdir -p "$temporaryDirectory/bin" "$temporaryDirectory/build with spaces"
export PATH="$temporaryDirectory/bin:$PATH"
client="$temporaryDirectory/bin/mariadb"
dump="$temporaryDirectory/bin/mariadb-dump"
cat > "$client" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
[[ " $* " == *' --no-defaults '* ]]
for arg in "$@"; do [[ "$arg" != *"$MOCK_SECRET"* ]] || exit 90; done
if [[ " $* " == *' --version '* ]]; then printf 'MariaDB fixture-version\n'; exit 0; fi
[[ " $* " == *' --user=root '* || " $* " == *' --user=taskcompass_dev '* ]] || exit 91
[[ " $* " != *'--database=taskcompass_integration'* ]]
if [[ " $* " == *' --user=root '* ]]; then
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
      [[ " $* " == *' --protocol=TCP '* && " $* " == *' --password '* ]]
   else
      [[ " $* " == *' --protocol=SOCKET '* && " $* " != *' --password '* ]]
   fi
fi
sql=$(cat)
[[ "$sql" != *'CREATE USER'* && "$sql" != *'CREATE DATABASE'* && "$sql" != *'DROP TABLE'* && "$sql" != *'GRANT SELECT ON'* ]] || exit 92
printf 'client\n' >> "$MOCK_DIRECTORY/calls"
case "$sql" in
   'SELECT DATABASE(),CURRENT_USER();') printf 'taskcompass_dev\ttaskcompass_dev@127.0.0.1\n' ;;
   'SELECT @@hostname,VERSION(),@@port;') printf 'fixture-host\t10.5.29-MariaDB\t3306\n' ;;
   *'AS authenticatedAccount'*) printf 'fixture-root-report\n' ;;
   *"'COUNT:SchemaMigration'"*)
      printf 'fixture-legacy\n'
      [[ "${MOCK_CURRENT_DRIFT:-0}" != 1 ]] || printf 'current-drift\n'
      if [[ "${MOCK_DRIFT:-0}" == 1 ]]; then
         if [[ -f "$MOCK_DIRECTORY/drift-seen" ]]; then printf 'drift\n'; fi
         : > "$MOCK_DIRECTORY/drift-seen"
      fi ;;
   *'SHOW CREATE TABLE SchemaMigration;'*) printf 'fixture-DDL\n' ;;
   *"SELECT 'Dataset',COUNT(*)"*) printf 'fixture-source\n' ;;
   *) exit 93 ;;
esac
CLIENT
cat > "$dump" <<'DUMP'
#!/usr/bin/env bash
set -euo pipefail
if [[ " $* " == *' --version '* ]]; then printf 'MariaDB fixture-dump-version\n'; exit 0; fi
printf 'dump\n' >> "$MOCK_DIRECTORY/calls"
[[ "${MOCK_DUMP_FAIL:-0}" != 1 ]] || exit 1
[[ " $* " == *' --user=taskcompass_dev '* && " $* " == *' --single-transaction '* && " $* " == *' --skip-add-drop-table '* ]]
for argument in "$@"; do
   if [[ "$argument" == --result-file=* ]]; then printf 'fixture SQL dump\n' > "${argument#*=}"; fi
done
DUMP
cat > "$temporaryDirectory/build with spaces/TaskCompassMigrate" <<'MIGRATE'
#!/usr/bin/env bash
set -euo pipefail
[[ "$*" == '--verify' || "$*" == '--verify --to-version 1' ]] || exit 94
[[ "$TASKCOMPASS_DB_NAME" == taskcompass_dev && "$TASKCOMPASS_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_DB_PORT" == 3306 && "$TASKCOMPASS_DB_ISOLATED" == 1 ]]
[[ "$TASKCOMPASS_DB_USER" == taskcompass_migrate || "$TASKCOMPASS_DB_USER" == taskcompass_app ]]
[[ "$TASKCOMPASS_DB_PASSWORD" == "$MOCK_SECRET" ]]
printf 'verify\n' >> "$MOCK_DIRECTORY/calls"
if [[ "${MOCK_FINAL_VERIFY_FAIL:-0}" == 1 ]]; then
   count=0
   [[ ! -f "$MOCK_DIRECTORY/final-verify-count" ]] || count=$(cat "$MOCK_DIRECTORY/final-verify-count")
   count=$((count+1))
   printf '%s\n' "$count" > "$MOCK_DIRECTORY/final-verify-count"
   [[ "$count" != 3 ]] || exit 1
fi
if [[ "${MOCK_VERIFY_FAIL:-0}" == 1 ]]; then printf 'fixture error %s\n' "$TASKCOMPASS_DB_PASSWORD" >&2; exit 1; fi
dataset=21be9da7-b767-4578-b972-cc9a35122ddb
[[ "${MSYSTEM:-}" != UCRT64 ]] || dataset=457bb52b-fea4-4347-9c47-675c6b035c90
[[ "${MOCK_WRONG_DATASET:-0}" != 1 ]] || dataset=00000000-0000-4000-8000-000000000001
version=1
[[ "${MOCK_WRONG_VERSION:-0}" != 1 ]] || version=2
printf 'Schema %s verified; dataset %s\n' "$version" "$dataset"
MIGRATE
cat > "$temporaryDirectory/bin/sudo" <<'SUDO'
#!/usr/bin/env bash
set -euo pipefail
[[ "$1" != -v ]] || exit 0
[[ "$1" == -n && "$2" == -- ]] || exit 95
shift 2
exec "$@"
SUDO
cat > "$temporaryDirectory/bin/less" <<'LESS'
#!/usr/bin/env bash
set -euo pipefail
printf 'pager\n' >> "$MOCK_DIRECTORY/calls"
[[ "$1" == -- && -s "$2" && "${LESSHISTFILE:-}" == - ]]
LESS
cat > "$temporaryDirectory/bin/stat" <<'STAT'
#!/usr/bin/env bash
# Mock Linux permission metadata even when the harness itself runs on Windows.
if [[ "${MOCK_DIRECTORY_PUBLIC:-0}" == 1 ]]; then printf '%s:755\n' "$EUID"
else printf '%s:700\n' "$EUID"; fi
STAT
cat > "$temporaryDirectory/bin/cygpath" <<'CYGPATH'
#!/usr/bin/env bash
printf '%s\n' "${@: -1}"
CYGPATH
cat > "$temporaryDirectory/bin/powershell.exe" <<'POWERSHELL'
#!/usr/bin/env bash
[[ "${MOCK_ACL_FAIL:-0}" != 1 ]]
POWERSHELL
chmod +x "$temporaryDirectory/bin/"* "$temporaryDirectory/build with spaces/TaskCompassMigrate"
options=(--client "$client" --dump "$dump" --build-dir "$temporaryDirectory/build with spaces")
run() { bash "$helperDirectory/$1.sh" "${options[@]}" "${@:2}"; }
inputs() {
   printf 'BASELINE UNCHANGED; WRITERS CLOSED\n%s\nALL TASKCOMPASS WRITERS CLOSED\n%s\n%s\n%s\n' "$MOCK_SECRET" "$MOCK_SECRET" "$MOCK_SECRET" "$MOCK_SECRET"
}
assertNoSecret() { [[ "$result" != *"$MOCK_SECRET"* ]]; }
for stage in 01-check 02-preflight 03-capture 03a-finish-capture 04-review; do
   run "$stage" --help >/dev/null
   run "$stage" --print-plan >/dev/null
done
[[ ! -e "$temporaryDirectory/calls" ]]
for stage in 02-preflight 03-capture 03a-finish-capture; do
   if result=$(run "$stage" 2>&1); then exit 1; fi
done
run 01-check >/dev/null
[[ ! -e "$temporaryDirectory/calls" ]]
if result=$(printf 'wrong\n' | run 03-capture --approved 2>&1); then exit 1; fi
[[ ! -e "$temporaryDirectory/calls" ]]
printf 'PASS: previews, tool checks and approval/confirmation gates cannot connect.\n'
run 02-preflight --approved >/dev/null
[[ "$(cat "$temporaryDirectory/calls")" == client ]]
: > "$temporaryDirectory/calls"
export MOCK_VERIFY_FAIL=1
if result=$(inputs | run 03-capture --approved --backup-dir "$temporaryDirectory/verify-failed" 2>&1); then exit 1; fi
assertNoSecret
[[ ! -e "$temporaryDirectory/verify-failed" ]]
unset MOCK_VERIFY_FAIL
export MOCK_WRONG_DATASET=1
if result=$(inputs | run 03-capture --approved --backup-dir "$temporaryDirectory/wrong-dataset" 2>&1); then exit 1; fi
[[ ! -e "$temporaryDirectory/wrong-dataset" ]]
unset MOCK_WRONG_DATASET
export MOCK_WRONG_VERSION=1
if result=$(inputs | run 03-capture --approved --backup-dir "$temporaryDirectory/wrong-version" 2>&1); then exit 1; fi
[[ ! -e "$temporaryDirectory/wrong-version" ]]
unset MOCK_WRONG_VERSION
printf 'PASS: verify/auth failure redacted and wrong dataset rejected before artifact creation.\n'
capture="$temporaryDirectory/private capture"
result=$(inputs | run 03-capture --approved --backup-dir "$capture" 2>&1)
assertNoSecret
[[ "$result" == *'PASS: B7.2 capture'* ]]
[[ "$("$originalStat" -c %a "$capture")" == 700 || "$(uname -s)" != Linux ]]
result=$(printf 'DUMP REVIEWED\n' | run 04-review --backup-dir "$capture" 2>&1)
[[ "$result" == *'owner-confirmed private review'* ]]
if result=$(inputs | run 03-capture --approved --backup-dir "$capture" 2>&1); then exit 1; fi
export MOCK_DIRECTORY_PUBLIC=1
if result=$(printf 'DUMP REVIEWED\n' | run 04-review --backup-dir "$capture" 2>&1); then exit 1; fi
unset MOCK_DIRECTORY_PUBLIC
printf 'PASS: real helper delegation, private evidence/hashes, paths with spaces, permission rejection and no overwrite.\n'
printf 'changed\n' >> "$capture/legacy-after.hex.tsv"
if result=$(printf 'DUMP REVIEWED\n' | run 04-review --backup-dir "$capture" 2>&1); then exit 1; fi
export MOCK_DRIFT=1
if result=$(inputs | run 03-capture --approved --backup-dir "$temporaryDirectory/drift" 2>&1); then exit 1; fi
[[ -s "$temporaryDirectory/drift/after-gui.sql" && ! -e "$temporaryDirectory/drift/b7-schema1-evidence.sha256" ]]
unset MOCK_DRIFT
export MOCK_DUMP_FAIL=1
if result=$(inputs | run 03-capture --approved --backup-dir "$temporaryDirectory/dump-failed" 2>&1); then exit 1; fi
[[ -d "$temporaryDirectory/dump-failed" && ! -e "$temporaryDirectory/dump-failed/b7-schema1-evidence.sha256" ]]
unset MOCK_DUMP_FAIL
printf 'PASS: drift, tampering and dump failure preserve artifacts and cannot seal/review.\n'
# Precisely reproduce final-verification failure after dump/exports completed.
export MOCK_FINAL_VERIFY_FAIL=1
partial="$temporaryDirectory/final verification failed"
if result=$(inputs | run 03-capture --approved --backup-dir "$partial" 2>&1); then exit 1; fi
unset MOCK_FINAL_VERIFY_FAIL
[[ -s "$partial/after-gui.sql" && ! -e "$partial/client-version.txt" && ! -e "$partial/b7-schema1-evidence.sha256" ]]
originalHash=$(sha256sum < "$partial/after-gui.sql")
finishInputs() { printf 'FINISH REVIEWED CAPTURE; WRITERS CLOSED\n%s\n' "$MOCK_SECRET"; }
if result=$(printf 'wrong\n' | run 03a-finish-capture --approved --backup-dir "$partial" 2>&1); then exit 1; fi
export MOCK_CURRENT_DRIFT=1
if result=$(finishInputs | run 03a-finish-capture --approved --backup-dir "$partial" 2>&1); then exit 1; fi
[[ ! -e "$partial/client-version.txt" && ! -e "$partial/b7-schema1-evidence.sha256" ]]
unset MOCK_CURRENT_DRIFT
export MOCK_VERIFY_FAIL=1
if result=$(finishInputs | run 03a-finish-capture --approved --backup-dir "$partial" 2>&1); then exit 1; fi
assertNoSecret
[[ ! -e "$partial/client-version.txt" && ! -e "$partial/b7-schema1-evidence.sha256" ]]
unset MOCK_VERIFY_FAIL
result=$(finishInputs | run 03a-finish-capture --approved --backup-dir "$partial" 2>&1)
[[ "$result" == *'PASS: reviewed B7.2 completion'* && "$(sha256sum < "$partial/after-gui.sql")" == "$originalHash" ]]
if result=$(finishInputs | run 03a-finish-capture --approved --backup-dir "$partial" 2>&1); then exit 1; fi
printf 'PASS: explicit final-verification-only completion gates, current-data drift/failed verify refusal, no dump overwrite or reseal.\n'
export MSYSTEM=UCRT64 MOCK_ACL_FAIL=1
if result=$(inputs | run 03-capture --approved --backup-dir "$temporaryDirectory/acl-failed" 2>&1); then exit 1; fi
[[ -d "$temporaryDirectory/acl-failed" && ! -e "$temporaryDirectory/acl-failed/dataset.txt" ]]
unset MOCK_ACL_FAIL
run 02-preflight --approved >/dev/null
windowsCapture="$temporaryDirectory/windows fixture"
result=$(inputs | run 03-capture --approved --backup-dir "$windowsCapture" 2>&1)
[[ "$result" == *'PASS: B7.2 capture'* ]]
result=$(printf '%s\nDUMP REVIEWED\n' "$windowsCapture" | run 04-review 2>&1)
[[ "$result" == *'owner-confirmed private review'* ]]
# A reviewed manifest cannot redirect checksum reads outside its known files.
printf '%064d  ../outside\n' 0 > "$windowsCapture/after-gui.sql.sha256"
if result=$(printf 'DUMP REVIEWED\n' | run 04-review --backup-dir "$windowsCapture" 2>&1); then exit 1; fi
[[ "$result" == *'Unexpected checksum path'* ]]
printf 'PASS: mocked Windows path/ACL flow, ACL failure before files, interactive directory and manifest path rejection.\n'
