#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fake clients/executables only. Never connects to a server.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
runner="$repositoryDirectory/scripts/phase11-validate.sh"
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
export MOCK_PHASE11_DIRECTORY="$temporaryDirectory"
unset MSYSTEM MYSQL_PWD TASKCOMPASS_DB_PASSWORD TASKCOMPASS_TEST_DB_PASSWORD
mkdir -p "$temporaryDirectory/tools with spaces" "$temporaryDirectory/build with spaces"
client="$temporaryDirectory/tools with spaces/mariadb"
buildDirectory="$temporaryDirectory/build with spaces"
cat > "$client" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
for argument in "$@"; do
   [[ "$argument" != *'fixture-secret'* ]] || exit 90
done
sql=$(cat)
printf 'client\n' >> "$MOCK_PHASE11_DIRECTORY/calls"
if [[ "${MOCK_CLIENT_FAIL:-0}" == 1 ]]; then
   printf 'mock failure password=%s\n' "$MYSQL_PWD" >&2
   exit 1
fi
if [[ "$sql" == *"CREATE USER 'taskcompass_migrate'"* ]]; then
   [[ "$sql" == *"IDENTIFIED BY 'fixture-secret-new-quote''&\slash';"* ]]
   touch "$MOCK_PHASE11_DIRECTORY/migrator"
elif [[ "$sql" == *"CREATE USER 'taskcompass_app'"* ]]; then
   touch "$MOCK_PHASE11_DIRECTORY/runtime"
elif [[ "$sql" == '-- mock dump'* ]]; then
   [[ "${MOCK_IMPORT_FAIL:-0}" != 1 ]] || exit 1
   touch "$MOCK_PHASE11_DIRECTORY/restored"
elif [[ "$sql" == 'SELECT @@hostname,VERSION(),@@port;' ]]; then
   printf 'fake-host\t13.0.2-MariaDB\t3306\r\n'
elif [[ "$sql" == 'SELECT VERSION();' ]]; then
   printf '13.0.2-MariaDB\r\n'
elif [[ "$sql" == 'SHOW GRANTS FOR PUBLIC;' ]]; then
   if [[ "${MOCK_PUBLIC_WRITE:-0}" == 1 ]]; then
      printf 'GRANT INSERT ON `taskcompass\\_dev`.* TO PUBLIC\n'
   else
      printf 'GRANT ALL PRIVILEGES ON `test`.* TO PUBLIC\nGRANT ALL PRIVILEGES ON `test\\_%%`.* TO PUBLIC\n'
   fi
elif [[ "$sql" == SELECT\ \(SELECT* ]]; then
   if [[ "${MOCK_NONEMPTY:-0}" == 1 ]]; then printf '1\t1\t0\t0\t0\n'
   else printf '1\t0\t0\t0\t0\n'; fi
elif [[ "$sql" == SELECT\ COUNT\(\*\)\ FROM\ mysql.user* ]]; then
   if [[ "${MOCK_EXISTING:-0}" == 1 ]]; then printf '1\n'; else printf '0\n'; fi
elif [[ "$sql" == SELECT\ Host\ FROM\ mysql.user* ]]; then
   printf '127.0.0.1\n'
elif [[ "$sql" == SELECT\ COUNT* ]]; then
   printf '0\n'
elif [[ "$sql" == SELECT\ CONCAT* ]]; then
   if [[ "$sql" == *"'taskcompass_test'"* ]]; then
      for right in SELECT INSERT UPDATE CREATE DROP ALTER; do
         printf 'schema|taskcompass\\_integration|%s|NO\n' "$right"
      done
   elif [[ "$sql" == *"'taskcompass_app'"* ]]; then
      printf '%s\n' 'table|task|SELECT|NO' 'table|task|INSERT|NO' 'table|task|UPDATE|NO' \
         'table|commandrecord|SELECT|NO' 'table|commandrecord|INSERT|NO' \
         'table|localchange|SELECT|NO' 'table|localchange|INSERT|NO' \
         'table|dataset|SELECT|NO' 'table|schemamigration|SELECT|NO'
   else
      printf 'schema|taskcompass\\_dev|SELECT|NO\n'
      if [[ "$sql" == *"'taskcompass_migrate'"* && ! -f "$MOCK_PHASE11_DIRECTORY/runtime" ]]; then
         printf '%s\n' 'schema|taskcompass\_dev|CREATE|NO' 'table|dataset|CREATE|NO' 'table|dataset|INSERT|NO' \
            'table|schemamigration|CREATE|NO' 'table|schemamigration|INSERT|NO'
      fi
   fi
   [[ "${MOCK_EXTRA_GRANT:-0}" != 1 ]] || printf 'unexpected-schema\n'
elif [[ "$sql" == 'SELECT DATABASE(),CURRENT_USER();' ]]; then
   user=''
   for argument in "$@"; do [[ "$argument" != --user=* ]] || user=${argument#--user=}; done
   database=taskcompass_dev
   [[ "$user" != taskcompass_test ]] || database=taskcompass_integration
   [[ "${MOCK_WRONG_IDENTITY:-0}" != 1 ]] || database=unrelated
   printf '%s\t%s@127.0.0.1\n' "$database" "$user"
elif [[ "$sql" == SELECT\ \'Dataset\',COUNT* ]]; then
   printf 'Dataset\t1\nTask\t2\nCommandRecord\t3\nLocalChange\t3\nSchemaMigration\t1\nstable escaped private fixture\n'
   if [[ "${MOCK_MISMATCH:-0}" == 1 && "$*" == *'taskcompass_integration'* ]]; then
      printf 'mismatch\n'
   fi
else
   printf 'Unexpected mock SQL\n' >&2
   exit 95
fi
CLIENT
cat > "$temporaryDirectory/tools with spaces/mariadb-dump" <<'DUMP'
#!/usr/bin/env bash
set -euo pipefail
[[ "$*" != *fixture-secret* ]]
[[ "${MOCK_DUMP_FAIL:-0}" != 1 ]] || exit 1
for argument in "$@"; do
   if [[ "$argument" == --result-file=* ]]; then
      printf '%s\n' '-- mock dump' 'CREATE TABLE mock();' > "${argument#--result-file=}"
   fi
done
DUMP
cat > "$buildDirectory/TaskCompassMigrate" <<'MIGRATE'
#!/usr/bin/env bash
set -euo pipefail
[[ "${MOCK_VERIFY_FAIL:-0}" != 1 ]] || exit 1
[[ "$*" != *'--apply'* ]] || touch "$MOCK_PHASE11_DIRECTORY/applied"
printf 'Schema 1 verified; dataset 21be9da7-b767-4578-b972-cc9a35122ddb\n'
MIGRATE
cat > "$buildDirectory/TaskCompass" <<'GUI'
#!/usr/bin/env bash
[[ "$TASKCOMPASS_DB_USER" == taskcompass_app ]]
GUI
cat > "$buildDirectory/MariaDbIntegrationTests" <<'INTEGRATION'
#!/usr/bin/env bash
set -euo pipefail
[[ "$*" == '--run --allow-reset' ]]
[[ "$TASKCOMPASS_TEST_DB_NAME" == taskcompass_integration && "$TASKCOMPASS_TEST_DB_USER" == taskcompass_test ]]
touch "$MOCK_PHASE11_DIRECTORY/integration-ran"
INTEGRATION
chmod +x "$client" "$temporaryDirectory/tools with spaces/mariadb-dump" "$buildDirectory/TaskCompassMigrate" "$buildDirectory/TaskCompass" "$buildDirectory/MariaDbIntegrationTests"
readonly PASSWORD='fixture-secret-account'
readonly NEW_PASSWORD="fixture-secret-new-quote'&\slash"
run() { bash "$runner" "$@" --client "$client" --build-dir "$buildDirectory"; }
reject() {
   if "$@" > "$temporaryDirectory/rejected.out" 2>&1; then
      printf 'FAIL: command unexpectedly succeeded\n' >&2; exit 1
   fi
   ! grep -q 'fixture-secret' "$temporaryDirectory/rejected.out"
}

# No-op defaults, previews and approval gates cannot invoke a client.
bash "$runner" >/dev/null
for action in integration-setup integration-test migration-setup migrate runtime-setup restore; do
   run "$action" --print-plan >/dev/null
   reject run "$action"
done
[[ ! -e "$temporaryDirectory/calls" ]]
reject run migration-setup --approved <<< 'wrong confirmation'
[[ ! -e "$temporaryDirectory/calls" ]]
printf 'PASS: no-op previews, explicit approvals and confirmation gates.\n'

# Before provisioning: existing-account, schema, PUBLIC and direct grants fail closed.
for condition in MOCK_EXISTING MOCK_NONEMPTY MOCK_PUBLIC_WRITE MOCK_EXTRA_GRANT MOCK_CLIENT_FAIL; do
   export "$condition=1"
   reject run migration-setup --approved --backup-dir "$temporaryDirectory/rejected-$condition" <<< $'PROVISION taskcompass_migrate\nfixture-secret-admin'
   unset "$condition"
done
[[ ! -e "$temporaryDirectory/migrator" ]]
printf 'PASS: existing accounts, nonempty schemas, unexpected grants/PUBLIC and redaction.\n'

integrationInput=$(printf '%s\n' 'TEST taskcompass_integration WITH DISPOSABLE RESET' "$PASSWORD" "$PASSWORD")
MOCK_NONEMPTY=1 reject run integration-test --approved <<< "$integrationInput"
MOCK_EXTRA_GRANT=1 reject run integration-test --approved <<< "$integrationInput"
[[ ! -e "$temporaryDirectory/integration-ran" ]]
run integration-test --approved <<< "$integrationInput" >/dev/null
[[ -f "$temporaryDirectory/integration-ran" ]]
printf 'PASS: live-test delegation follows grant/catalog checks and uses isolated settings.\n'

# Full fake migration-account/baseline path, including password SQL escaping.
baseline="$temporaryDirectory/baseline with spaces"
run migration-setup --approved --backup-dir "$baseline" <<< "$(printf '%s\n' 'PROVISION taskcompass_migrate' "$PASSWORD" "$PASSWORD" "$NEW_PASSWORD" "$NEW_PASSWORD")" > "$temporaryDirectory/setup.out"
[[ -f "$temporaryDirectory/migrator" && -s "$baseline/before-migration.sql" ]]
! grep -q 'fixture-secret' "$temporaryDirectory/setup.out"
run migrate --approved --backup-dir "$baseline" <<< "$(printf '%s\n' 'MIGRATE taskcompass_dev' "$PASSWORD" "$PASSWORD")" >/dev/null
[[ -f "$temporaryDirectory/applied" ]]
run runtime-setup --approved <<< "$(printf '%s\n' 'PROVISION taskcompass_app AND REVOKE migrator writes' "$PASSWORD" "$PASSWORD" "$NEW_PASSWORD" "$NEW_PASSWORD")" >/dev/null
run gui <<< "$(printf '%s\n' "$PASSWORD" "$PASSWORD")" >/dev/null
MOCK_VERIFY_FAIL=1 reject run gui <<< "$(printf '%s\n' "$PASSWORD" "$PASSWORD")"
printf 'PASS: fake baseline, migration, runtime grants/revocation and GUI configuration.\n'

# Populated backup and safe negative paths.
snapshot="$temporaryDirectory/populated with spaces"
run backup --backup-dir "$snapshot" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")" >/dev/null
[[ -s "$snapshot/after-gui.sql.sha256" && -s "$snapshot/source.tsv.sha256" ]]
reject run backup --backup-dir "$snapshot" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD")"
MOCK_WRONG_IDENTITY=1 reject run backup --backup-dir "$temporaryDirectory/wrong-identity" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")"
MOCK_DUMP_FAIL=1 reject run backup --backup-dir "$temporaryDirectory/dump-failure" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")"
hash=$(sha256sum "$snapshot/after-gui.sql"); hash=${hash%% *}
restoreInput=$(printf '%s\n' "RESTORE taskcompass_integration $hash" 'DUMP REVIEWED; WRITERS CLOSED' "$PASSWORD" "$PASSWORD")
MOCK_NONEMPTY=1 reject run restore --approved --backup-dir "$snapshot" <<< "$restoreInput"
[[ ! -e "$temporaryDirectory/restored" ]]
MOCK_IMPORT_FAIL=1 reject run restore --approved --backup-dir "$snapshot" <<< "$restoreInput"
[[ ! -e "$temporaryDirectory/restored" ]]
run restore --approved --backup-dir "$snapshot" <<< "$restoreInput" >/dev/null
[[ -f "$temporaryDirectory/restored" ]]
run compare --backup-dir "$snapshot" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")" > "$temporaryDirectory/compare.out"
grep -q 'PASS: schema/dataset and all five tables' "$temporaryDirectory/compare.out"
reject run compare --backup-dir "$snapshot" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD")"
printf 'tampered\n' >> "$snapshot/after-gui.sql"
reject run restore --approved --backup-dir "$snapshot" <<< "$restoreInput"
printf 'PASS: fake populated backup, isolated restore, complete comparison and immutable evidence guards.\n'

secondSnapshot="$temporaryDirectory/mismatch snapshot"
run backup --backup-dir "$secondSnapshot" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")" >/dev/null
MOCK_MISMATCH=1 reject run compare --backup-dir "$secondSnapshot" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")"
grep -q 'Restored data differs' "$temporaryDirectory/rejected.out"
[[ -s "$secondSnapshot/restored.tsv" ]]
printf 'PASS: mismatched restore data fails and preserves comparison evidence.\n'

# Exercise Windows dispatch/ACL refusal with fake built-in tools on Linux only.
# This does not establish real PowerShell or Windows filesystem behavior.
if [[ "$(uname -s)" == Linux ]]; then
   cat > "$temporaryDirectory/tools with spaces/cygpath" <<'CYGPATH'
#!/usr/bin/env bash
printf '%s\n' "$2"
CYGPATH
   cat > "$temporaryDirectory/tools with spaces/powershell.exe" <<'POWERSHELL'
#!/usr/bin/env bash
[[ -n "$TASKCOMPASS_PRIVATE_DIRECTORY" ]]
[[ "${MOCK_ACL_FAIL:-0}" != 1 ]]
POWERSHELL
   chmod +x "$temporaryDirectory/tools with spaces/cygpath" "$temporaryDirectory/tools with spaces/powershell.exe"
   MSYSTEM=UCRT64 PATH="$temporaryDirectory/tools with spaces:$PATH" MOCK_ACL_FAIL=1 \
      reject run backup --backup-dir "$temporaryDirectory/windows-acl-failure" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD")"
   [[ ! -e "$temporaryDirectory/windows-acl-failure/after-gui.sql" ]]
   MSYSTEM=UCRT64 PATH="$temporaryDirectory/tools with spaces:$PATH" \
      run backup --backup-dir "$temporaryDirectory/windows-dispatch" <<< "$(printf '%s\n' 'ALL TASKCOMPASS WRITERS CLOSED' "$PASSWORD" "$PASSWORD")" >/dev/null
   [[ -s "$temporaryDirectory/windows-dispatch/after-gui.sql" ]]
   printf 'PASS: fake UCRT64 path dispatch and ACL failure stop before dump.\n'
fi
