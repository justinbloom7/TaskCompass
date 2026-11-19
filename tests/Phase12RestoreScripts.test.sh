#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Offline fakes only: even the pinned private snapshot digests are simulated.
set -euo pipefail
umask 077
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
helperDirectory="$repositoryDirectory/scripts/phase12/b73"
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
export MOCK_DIRECTORY="$temporaryDirectory" MOCK_SECRET='b73-fixture-secret'
trap 'code=$?; printf "Offline fixture failure in %s at line %s (exit %s).\n" "${BASH_SOURCE[0]}" "$LINENO" "$code" >&2; if [[ -n "${result:-}" ]]; then printf "%s\n" "${result//${MOCK_SECRET:-unused}/[fixture-secret]}" >&2; fi' ERR
export MSYSTEM=''
unset MYSQL_PWD TASKCOMPASS_PHASE12_TEST_DB_PASSWORD
mkdir -p "$temporaryDirectory/bin" "$temporaryDirectory/build with spaces"
export PATH="$temporaryDirectory/bin:$PATH"
testPython=${TASKCOMPASS_TEST_PYTHON:-$(command -v python3 || command -v python)}
export TASKCOMPASS_TEST_PYTHON="$testPython"
cat > "$temporaryDirectory/bin/sha256sum" <<'WRAPPER'
#!/usr/bin/env bash
exec "$TASKCOMPASS_TEST_PYTHON" "$MOCK_DIRECTORY/hash.py" "$@"
WRAPPER
cat > "$temporaryDirectory/hash.py" <<'HASH'
#!/usr/bin/env python3
# Only exact synthetic marker bytes simulate approved private snapshot hashes.
import hashlib, pathlib, re, sys
sys.stdout.reconfigure(encoding="utf-8", newline="\n")
pins = {b'fixture-reviewed-linux\n': '83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26',
        b'fixture-reviewed-windows\n': '6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50'}
def digest(data):
   return pins.get(data, hashlib.sha256(data).hexdigest())
if sys.argv[1:2] == ['--check']:
   for manifest in sys.argv[2:]:
      for line in pathlib.Path(manifest).read_text(encoding="utf-8").splitlines():
         match = re.fullmatch(r'([0-9a-f]{64}) [ *](.+)', line)
         if not match or digest(pathlib.Path(match[2]).read_bytes()) != match[1]:
            sys.exit(1)
         print(match[2] + ': OK')
else:
   for name in sys.argv[1:] or ['-']:
      data = sys.stdin.buffer.read() if name == '-' else pathlib.Path(name).read_bytes()
      print(digest(data) + '  ' + name)
HASH
cat > "$temporaryDirectory/bin/mariadb" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
if [[ " $* " == *' --version '* ]]; then printf 'fixture%s\n' "${MOCK_VERSION_SUFFIX:-}"; exit 0; fi
[[ " $* " == *' --database=taskcompass_phase12_test '* ]]
[[ " $* " != *' --force '* ]]
for arg in "$@"; do [[ "$arg" != *"$MOCK_SECRET"* ]] || exit 90; done
sql=$(cat)
printf 'client\n' >> "$MOCK_DIRECTORY/calls"
if [[ "$sql" == *"SELECT 'SERVER'"* ]]; then
   [[ " $* " == *' --user=root '* ]]
   host=feledria; version=10.5.29-MariaDB
   [[ "${MSYSTEM:-}" != UCRT64 ]] || { host=feldria-w10; version=13.0.2-MariaDB; }
   [[ "${MOCK_WRONG_SERVER:-0}" != 1 ]] || host=unexpected
   tables=0; [[ ! -e "$MOCK_STATE" ]] || tables=5
   [[ "${MOCK_NONEMPTY:-0}" != 1 ]] || tables=1
   tableCase=0; [[ "${MSYSTEM:-}" != UCRT64 ]] || tableCase=1
   printf 'SERVER\t%s\t%s\t3306\ttaskcompass_phase12_test\troot@localhost\t%s\t1\t1\n' "$host" "$version" "$tableCase"
   printf 'EMPTY\t1\t%s\t0\t0\t0\t0\n' "$tables"
   printf 'HOST\t127.0.0.1\nGLOBAL\t0\nTABLE\t0\nCOLUMN\t0\nROLE\t%s\n' "${MOCK_ROLE:-0}"
   for right in SELECT INSERT UPDATE CREATE DROP ALTER; do printf 'RIGHT\ttaskcompass\\_phase12\\_test\t%s\tNO\n' "$right"; done
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
      if [[ "${MOCK_PUBLIC_UNSAFE:-0}" == 1 ]]; then printf 'GRANT INSERT ON `taskcompass%%`.* TO PUBLIC\n'
      else printf 'GRANT ALL PRIVILEGES ON `test\\_%%`.* TO PUBLIC\n'; fi
   fi
   exit 0
fi
[[ " $* " == *' --user=taskcompass_phase12_test '* && "$MYSQL_PWD" == "$MOCK_SECRET" ]]
case "$sql" in
   'SELECT DATABASE(),CURRENT_USER();') printf 'taskcompass_phase12_test\ttaskcompass_phase12_test@127.0.0.1\n' ;;
   fixture-reviewed-*)
      printf 'import\n' >> "$MOCK_DIRECTORY/calls"
      : > "$MOCK_STATE"
      [[ "${MOCK_IMPORT_FAIL:-0}" != 1 ]] || exit 1 ;;
   *"'COUNT:SchemaMigration'"*) printf 'fixture-legacy\n' ;;
   *'SHOW CREATE TABLE SchemaMigration;'*) printf 'fixture-DDL\n' ;;
   *) exit 91 ;;
esac
CLIENT
cat > "$temporaryDirectory/build with spaces/TaskCompassMigrate" <<'MIGRATE'
#!/usr/bin/env bash
set -euo pipefail
[[ "$*" == '--verify --phase12-test --to-version 1' ]]
[[ "$TASKCOMPASS_PHASE12_TEST_DB_NAME" == taskcompass_phase12_test && "$TASKCOMPASS_PHASE12_TEST_DB_USER" == taskcompass_phase12_test && "$TASKCOMPASS_PHASE12_TEST_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_PHASE12_TEST_DB_PORT" == 3306 && "$TASKCOMPASS_PHASE12_TEST_DB_ISOLATED" == 1 && "$TASKCOMPASS_PHASE12_TEST_DB_PASSWORD" == "$MOCK_SECRET" ]]
dataset=21be9da7-b767-4578-b972-cc9a35122ddb
[[ "${MSYSTEM:-}" != UCRT64 ]] || dataset=457bb52b-fea4-4347-9c47-675c6b035c90
[[ "${MOCK_WRONG_DATASET:-0}" != 1 ]] || dataset=00000000-0000-4000-8000-000000000001
printf 'Schema 1 verified; dataset %s\n' "$dataset"
MIGRATE
cat > "$temporaryDirectory/bin/sudo" <<'SUDO'
#!/usr/bin/env bash
[[ "$1" != -v ]] || exit 0
[[ "$1" == -n && "$2" == -- ]] || exit 92
shift 2
exec "$@"
SUDO
cat > "$temporaryDirectory/bin/stat" <<'STAT'
#!/usr/bin/env bash
printf '%s:700\n' "$EUID"
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
fixture() {
   snapshot="$temporaryDirectory/$1"
   mkdir -- "$snapshot"
   platform=linux; dataset=21be9da7-b767-4578-b972-cc9a35122ddb
   [[ "${MSYSTEM:-}" != UCRT64 ]] || { platform=windows; dataset=457bb52b-fea4-4347-9c47-675c6b035c90; }
   printf 'fixture-reviewed-%s\n' "$platform" > "$snapshot/after-gui.sql"
   printf 'fixture-legacy\n' > "$snapshot/legacy-before.hex.tsv"
   cp "$snapshot/legacy-before.hex.tsv" "$snapshot/legacy-after.hex.tsv"
   printf 'fixture-DDL\n' > "$snapshot/schema1.ddl.txt"
   printf 'fixture-source\n' > "$snapshot/source.tsv"
   cp "$snapshot/source.tsv" "$snapshot/source-after.tsv"
   printf '%s\n' "$dataset" > "$snapshot/dataset.txt"
   for name in server.txt client-version.txt dump-version.txt; do printf 'fixture\n' > "$snapshot/$name"; done
   (cd -- "$snapshot"
    sha256sum after-gui.sql > after-gui.sql.sha256
    sha256sum source.tsv > source.tsv.sha256
    sha256sum legacy-before.hex.tsv legacy-after.hex.tsv schema1.ddl.txt source-after.tsv dataset.txt server.txt client-version.txt dump-version.txt > b7-schema1-evidence.sha256)
   export MOCK_STATE="$temporaryDirectory/$1-state"
}
run() { bash "$helperDirectory/$1.sh" --client "$temporaryDirectory/bin/mariadb" --build-dir "$temporaryDirectory/build with spaces" --backup-dir "$snapshot" "${@:2}"; }
inputs() { printf 'RESTORE taskcompass_phase12_test\nDUMP REVIEWED; WRITERS CLOSED\n%s\n' "$MOCK_SECRET"; }
# Ensure root preflight stays SELECT-only, independently of the fake client.
"$TASKCOMPASS_TEST_PYTHON" - "$helperDirectory/preflight.sql" <<'READONLY'
import pathlib, re, sys
source = re.sub(r'^--.*$', '', pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"), flags=re.M)
assert all(statement.strip().startswith('SELECT ') for statement in source.split(';') if statement.strip())
READONLY
# Reusable offline-only fixture factory; never a production script bypass.
if [[ "${TASKCOMPASS_OPERATOR_FIXTURES_ONLY:-0}" == 1 ]]; then return 0; fi
fixture 'linux snapshot'
for stage in 01-preflight 02-import 03-compare; do
   run "$stage" --help >/dev/null
   run "$stage" --print-plan >/dev/null
   if result=$(run "$stage" 2>&1); then exit 1; fi
done
[[ ! -e "$temporaryDirectory/calls" ]]
export MOCK_VERSION_SUFFIX=changed
if result=$(run 01-preflight --approved 2>&1); then exit 1; fi
[[ ! -e "$temporaryDirectory/calls" ]]
unset MOCK_VERSION_SUFFIX
run 01-preflight --approved >/dev/null
for failureMode in MOCK_NONEMPTY MOCK_WRONG_SERVER MOCK_ROLE; do
   export "$failureMode=1"
   if result=$(inputs | run 02-import --approved 2>&1); then exit 1; fi
   [[ ! -e "$snapshot/b73-import-attempt.txt" && ! -e "$MOCK_STATE" ]]
   unset "$failureMode"
done
printf 'PASS: previews/approval gates; nonempty/wrong-server/extra-role refusal before import.\n'
result=$(inputs | run 02-import --approved 2>&1)
[[ "$result" == *'PASS: one-shot import exit 0'* && "$result" != *"$MOCK_SECRET"* ]]
if result=$(inputs | run 02-import --approved 2>&1); then exit 1; fi
result=$(printf '%s\n' "$MOCK_SECRET" | run 03-compare --approved 2>&1)
[[ "$result" == *'PASS: B7.3 schema1/UUID'* && -s "$snapshot/b73-comparison.sha256" ]]
if result=$(printf '%s\n' "$MOCK_SECRET" | run 03-compare --approved 2>&1); then exit 1; fi
printf 'PASS: exact pinned snapshot, one-shot import, guarded Qt verify and no-overwrite complete comparison.\n'
fixture 'failed import'
export MOCK_IMPORT_FAIL=1
if result=$(inputs | run 02-import --approved 2>&1); then exit 1; fi
unset MOCK_IMPORT_FAIL
[[ -s "$snapshot/b73-import-attempt.txt" && ! -e "$snapshot/b73-import-success.txt" && -e "$MOCK_STATE" ]]
if result=$(inputs | run 02-import --approved 2>&1); then exit 1; fi
if result=$(printf '%s\n' "$MOCK_SECRET" | run 03-compare --approved 2>&1); then exit 1; fi
fixture 'tampered snapshot'
printf 'tampered\n' >> "$snapshot/after-gui.sql"
if result=$(run 01-preflight --approved 2>&1); then exit 1; fi
[[ ! -e "$MOCK_STATE" ]]
fixture 'valid checksum wrong approval pin'
printf 'unapproved payload\n' > "$snapshot/after-gui.sql"
(cd -- "$snapshot" && sha256sum after-gui.sql > after-gui.sql.sha256)
if result=$(run 01-preflight --approved 2>&1); then exit 1; fi
[[ "$result" == *'owner-approved platform hash'* && ! -e "$MOCK_STATE" ]]
printf 'PASS: partial failure attempt marker retained; no reimport/reset; tampering and valid-but-unapproved hash refused.\n'
export MSYSTEM=UCRT64
fixture 'windows snapshot'
export MOCK_PUBLIC_UNSAFE=1
if result=$(run 01-preflight --approved 2>&1); then exit 1; fi
unset MOCK_PUBLIC_UNSAFE
run 01-preflight --approved >/dev/null
result=$(inputs | run 02-import --approved 2>&1)
[[ "$result" == *'PASS: one-shot import exit 0'* ]]
export MOCK_WRONG_DATASET=1
if result=$(printf '%s\n' "$MOCK_SECRET" | run 03-compare --approved 2>&1); then exit 1; fi
[[ ! -e "$snapshot/rehearsal1.hex.tsv" ]]
unset MOCK_WRONG_DATASET
result=$(printf '%s\n' "$MOCK_SECRET" | run 03-compare --approved 2>&1)
[[ "$result" == *'PASS: B7.3 schema1/UUID'* ]]
printf 'PASS: mocked Windows pinned snapshot/ACL, PUBLIC scope enforcement and wrong-dataset refusal.\n'
