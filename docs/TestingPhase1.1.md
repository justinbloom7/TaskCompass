<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Testing Phase 1.1

For Windows, prefer the [scripted one-step-at-a-time guide](WindowsPhase1.1.md).
The detailed manual procedure below remains available for inspection and Linux.

This is an operator walkthrough. The authoritative approval record and exact
preflight/grant checks are in [Phase1.1.md](milestones/Phase1.1.md), under
**Approval Requested — Stage 1 after safety review**. Preparing these instructions
is not approval for Codex to execute live SQL. Nothing below has been run against
a database by the agent as part of writing this guide.

Reusable setup/environment/test scripts now live in
[temp/testdb](../temp/testdb/README.md). Start with
`bash temp/testdb/setup.sh --print-plan` to inspect placeholder SQL without a
connection. Its README provides the guarded setup and test commands. Creating
those helpers did not approve their live execution.

`--print-plan` shows the provisioning SQL template with a password placeholder.
It does not connect, check existing resources, show the script's preflight query,
prompt for passwords, apply table migrations or run tests. It is SQL inspection,
not a complete dry-run validation of the server. The steps below cover the rest.

**Current checkpoint (2026-11-13):** the owner reported a successful guarded
integration run with exit 0 and an empty test schema. Platform/server details are
pending confirmation; this is not a claim that both platforms passed. Do not
repeat fresh provisioning on that server. The next review is **Stage 2A development
foundation** in [Phase1.1.md](milestones/Phase1.1.md), followed later by the separately
approved Stage 2B populated backup/restore rehearsal. The instructions below remain
useful for an unvalidated platform; Stage 1 completion is not final acceptance.

There are two separate steps: Stage 1 disposable integration tests first; Stage 2
migration of taskcompass_dev for actual persistent GUI testing after separate
approval. The existing SELECT-only taskcompass_dev account cannot run migrations
or save tasks and must remain unchanged.

## 1. Build and run database-free tests

From the repository root on Linux:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Expected current result: nine tests pass and MariaDbIntegrationTests is skipped
(including the database-free IntegrationDatabaseScripts mock suite).
This validates core/fake-repository GUI/safety behavior, not live persistence.
Keep live CTest registration OFF; run the live executable explicitly later.

Windows: open MSYS2 UCRT64, run `source scripts/dev-environment.sh --no-password`,
then use `build-ucrt64` instead of `build` in these commands. Windows results for
Phase 1.1 still need to be reported independently.

## 2. Check that MariaDB is running

Linux:

```bash
systemctl status mariadb --no-pager
ss -ltn 'sport = :3306'
```

If stopped, the operator can start it with:

```bash
sudo systemctl start mariadb
```

On Windows, start the installed MariaDB service in Services. See
[Installation.md](Installation.md) for server prerequisites. ERROR 2002 before
connection is a service/listener issue, not evidence of a wrong password. No new
server installation is needed if the existing MariaDB service works.

## 3. Open an administrator client for Stage 1 setup

Use an authorized administrator only for setup; the test executable uses its own
restricted account. On Linux, with local root socket authentication:

```bash
sudo env MYSQL_HISTFILE=/dev/null MARIADB_HISTFILE=/dev/null \
   mariadb --no-defaults --protocol=SOCKET --user=root
```

If administration instead requires a database password/TCP login:

```bash
MYSQL_HISTFILE=/dev/null MARIADB_HISTFILE=/dev/null \
   mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=root --password
```

This prompts for the **administrator database password**, not the new test password.
The Linux socket command's sudo prompt is for your Linux password. `mysql` may be
used in place of `mariadb` if that is the installed client name. --no-defaults avoids
reading unrelated application client option files.

For the native Windows client from UCRT64, use `MYSQL_HISTFILE=NUL
MARIADB_HISTFILE=NUL` before the TCP command; replace `mariadb` with the full quoted
path to mariadb.exe if needed. NUL is the native Windows null device.
Both history variables are set so an existing MariaDB-specific override cannot
re-enable history. See [MariaDB client documentation](https://mariadb.com/docs/server/clients-and-utilities/mariadb-client/mariadb-command-line-client)
for client history handling.

## 4. Check before creating anything

In the administrator client, execute the exact read-only administrator preflight
from the milestone. At minimum, the database/account existence checks are:

```sql
SELECT VERSION();
SELECT SCHEMA_NAME FROM information_schema.SCHEMATA
   WHERE SCHEMA_NAME='taskcompass_integration';
SELECT User,Host FROM mysql.user WHERE User='taskcompass_test';
```

For a fresh setup, the last two queries return no rows. **If either resource exists,
stop and inspect it; do not drop it, change its password, or recreate it blindly.**
The full milestone preflight also checks tables/views, routines, triggers, events,
and inherited privilege policy. Never select or change an unrelated database.

## 5. Create the isolated test database/account

Only for confirmed absent resources and the reviewed Stage 1 setup:

```sql
CREATE DATABASE taskcompass_integration
   CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_test'@'127.0.0.1'
   IDENTIFIED BY '<distinct-strong-test-password>';
GRANT SELECT, INSERT, UPDATE, CREATE, DROP, ALTER
   ON `taskcompass\_integration`.*
   TO 'taskcompass_test'@'127.0.0.1';
```

Replace the placeholder privately with a strong, distinct password saved in your
password manager. A randomly generated 32+ character letters/digits password avoids
SQL quote/backslash escaping complications. The password-bearing SQL is entered
only in the client with history disabled, never in a source file or shell command.
Do not record/share that session or credentials. Follow the milestone's private
stdin/log-handling procedure if Codex performs provisioning after approval.

Run the milestone's exact privilege-metadata checks afterward. Expected: the six
schema-scoped privileges above, no global/grant authority or extra explicit roles;
review PUBLIC only on server versions that support it (not MariaDB 10.5). Do not
broaden privileges to fix an unexplained error. Exit the client with `quit`.

For convenience, those checks in the administrator client are:

```sql
SET @testGrantee = CONCAT(CHAR(39),'taskcompass_test',CHAR(39),'@',CHAR(39),'127.0.0.1',CHAR(39));
SELECT PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.USER_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT TABLE_SCHEMA,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.SCHEMA_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.TABLE_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,PRIVILEGE_TYPE,IS_GRANTABLE FROM information_schema.COLUMN_PRIVILEGES
   WHERE GRANTEE=@testGrantee;
SELECT Host,User,Role FROM mysql.roles_mapping WHERE User='taskcompass_test';
```

The first result should be empty or USAGE with NO grant option. Schema privileges
should contain only SELECT/INSERT/UPDATE/CREATE/DROP/ALTER on the exact escaped
integration name, all IS_GRANTABLE=NO. Table/column privilege and explicit-role
results should be empty. Unexpected results require review rather than repair.
On MariaDB 13.0.2 (or another 10.11+ server), additionally inspect:

```sql
SHOW GRANTS FOR PUBLIC;
```

Skip that last query on Linux MariaDB 10.5. Follow inherited roles for review if
PUBLIC has them; do not assume direct grants alone establish effective rights.
Do not publish authentication material or change PUBLIC policy to run these tests.

Do **not** run scripts/dev-database.sh setup for this step: that helper manages the
older diagnostic database/account, not taskcompass_integration.

## 6. Optional manual TCP authentication check

Use the **new test password** at the hidden prompt:

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_test --password taskcompass_integration
```

Inside the client:

```sql
SELECT VERSION(), DATABASE(), CURRENT_USER();
SHOW TABLES;
```

Expected database: taskcompass_integration; account: taskcompass_test@127.0.0.1;
no tables. Do not manually apply the migration or create a probe table. The suite
must start empty and performs its own migration. Exit with `quit`.

## 7. Run the live integration suite

In Bash on Linux or UCRT64, with shell tracing disabled:

```bash
set +x
export TASKCOMPASS_TEST_DB_HOST=127.0.0.1
export TASKCOMPASS_TEST_DB_PORT=3306
export TASKCOMPASS_TEST_DB_NAME=taskcompass_integration
export TASKCOMPASS_TEST_DB_USER=taskcompass_test
export TASKCOMPASS_TEST_DB_ISOLATED=1
read -r -s -p 'Integration database password: ' TASKCOMPASS_TEST_DB_PASSWORD
printf '\n'
export TASKCOMPASS_TEST_DB_PASSWORD
./build/MariaDbIntegrationTests --run --allow-reset
testExitCode=$?
unset TASKCOMPASS_TEST_DB_PASSWORD
printf 'Integration test exit code: %s\n' "$testExitCode"
```

Windows changes only the executable line to:

```bash
./build-ucrt64/MariaDbIntegrationTests.exe --run --allow-reset
```

Enter the taskcompass_test password, **not** the diagnostic, administrator or Linux
password. The executable reads TASKCOMPASS_TEST_DB_* only; sourcing the older
DB environment helper does not configure this test account.

The test intentionally creates, mutates and drops its known test tables inside
this disposable schema. It validates migrations, partial-initialization refusal,
version/checksum/schema drift checks, task persistence, lifecycle, undo/redo,
transaction rollback and new-connection reload. It refuses existing tables even
if they have familiar names, and never resets taskcompass_dev.

Success prints `PASS: MariaDB migrations/version/checksum/fingerprint, ...` and
`Test schema left empty.` with exit code **0**. Missing opt-in exits **77**; a
failure exits **1** and retains evidence. Unset the password after either result.

## 8. If it fails, stop and preserve the output

| Result | Next step |
| --- | --- |
| Cannot open MariaDB | Check service/3306 first, then test-account authentication and Qt QMYSQL availability. |
| Invalid isolated configuration | Check all TEST variable names, exact database/user and ISOLATED=1. |
| Identity guard failed | Confirm TCP and the exact account host; do not bypass validation. |
| Database must start completely empty | Leftovers/pre-existing objects require inspection and separately reviewed cleanup, even with --allow-reset. |
| Another process owns the schema | Let the other approved run finish; do not remove locks or run competing migrations. |
| Migration/query failure | Preserve error text and current test state. Do not add global privileges or repeatedly rerun setup. |

Share command output and the exit code, with passwords/credentials omitted. A
successful build is not evidence that any of these live checks passed.

## 9. Persistent GUI testing comes after Stage 1

After the integration suite passes, record its Linux/Windows results in the
milestone. Prepare and separately approve Stage 2: backup/recovery validation,
new narrowly scoped development migration/runtime accounts, explicit migration
of taskcompass_dev and runtime grants. Do not grant writes to taskcompass_dev's
existing diagnostic account.

Only after Stage 2 approval and setup can `./build/TaskCompass` (or the Windows
.exe) test real persistent GUI work. `TaskCompassPrototype` still runs without a
server but its fixture edits are deliberately not persistent. Do not use the
migration CLI --apply manually before the Stage 2 checkpoint.

Final acceptance also requires real capture/edit/completion/reopen, Show Completed,
all supported undo/redo operations, close/restart persistence, Linux and Windows
validation and the reviewed backup/restore checks. This guide does not mark any
pending check passed or authorize Phase 1.2.

Whole-milestone sequence: database-free tests → review/approve Stage 1 → check the
service and privileged preflight → fresh integration provisioning → verify effective
grants → live integration suite → review results → finalize/approve Stage 2 → verify
backup/recovery prerequisites and migrate development with separate accounts →
test persistent GUI and all supported undo/redo operations → close/restart checks →
complete the approved backup/restore validation → independently repeat required
Linux/Windows checks → owner review and final acceptance. A build, mock test or
successful Stage 1 alone does not complete the milestone.

## 10. Stage 2B explained: restore a populated backup into the test database

**Latest result (2026-11-14): Stage 2B owner-verified pass.** Import and restored
schema verification exited 0, the dataset UUID matched, and B5 full five-table
exports had identical hashes with comparison exit 0. Preserve the restored test
database and private backups; no cleanup is authorized. Independent Windows
Phase 1.1 validation and final milestone acceptance remain pending. The walkthrough
below describes the completed procedure, not instructions to repeat the import.

**Earlier resume checkpoint (superseded): B3, before restore.** The owner completed GUI
checks, created and inspected a populated backup with exit 0, and reported all
four administrator target-emptiness queries empty. The chosen backup is
/home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql,
SHA-256 f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c.
Do not recreate this backup just to resume. The last unresolved item was the lost
taskcompass_test password; reset instructions are recorded at the milestone's end,
but completion and successful test-account login have not been reported. Resume
with that login/identity check and current destination preflight, then the
separate restore approval. No restore has been reported. If development changed
after the backup, retain this artifact but plan a new quiescent snapshot for an
exact source/restore comparison rather than assuming current source data matches.

Stage 2A proved that the application saves and reloads tasks. Stage 2B proves that
a backup can recover those tasks **and their history**. The source is
taskcompass_dev; the destination is taskcompass_integration. Development is never
overwritten. The complete approval procedure remains in the milestone under
**Stage 2B restore rehearsal — prepared procedure, later exact-file approval**.

### B1. Close the GUI and verify the source

Close TaskCompass and leave it closed until the backup and source/restore
comparison finish. Stop other TaskCompass writers too. Verify schema 1 using
TaskCompassMigrate --verify with subsection G's runtime environment/password,
without launching the GUI. Record the dataset UUID and exit status. This step
does not apply a migration. Do not use --apply for a restore rehearsal.

### B2. Back up the populated development database

Yes: reuse subsection B's dump options, changing the output filename to after-gui.sql.
Use the **diagnostic account password** here, not the runtime, migration or root
password. This complete Linux command creates a fresh directory for each attempt:

```bash
taskCompassBackupDirectory="$HOME/temp/TaskCompassBackups/Phase1.1-restore-$(date +%Y%m%d-%H%M%S)"
(
   umask 077
   mkdir -p -- "$(dirname "$taskCompassBackupDirectory")" &&
   mkdir -- "$taskCompassBackupDirectory" &&
   mariadb-dump --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
      --user=taskcompass_dev --password --default-character-set=utf8mb4 \
      --single-transaction --skip-lock-tables --no-tablespaces --skip-triggers \
      --skip-add-drop-table --skip-add-locks --skip-disable-keys \
      --no-create-db --hex-blob \
      --result-file="$taskCompassBackupDirectory/after-gui.sql" taskcompass_dev
)
backupExitCode=$?
printf 'Backup exit code: %s\n' "$backupExitCode"
```

Stop unless the exit code is zero. Then check the file:

```bash
test -s "$taskCompassBackupDirectory/after-gui.sql"
printf 'Nonempty-file check exit code: %s\n' "$?"
sha256sum "$taskCompassBackupDirectory/after-gui.sql"
less "$taskCompassBackupDirectory/after-gui.sql"
```

Both exit codes must be zero. Unlike before-migration.sql, this dump should contain
CREATE TABLE and INSERT statements for the five application tables, including
your tasks and history. It must not contain account changes, DROP statements,
USE/CREATE DATABASE, unrelated schemas or stored code. Review session-setting
save/restore statements as described in the milestone. Keep the dump private:
it contains task titles, notes and audit snapshots. Do not paste it into chat.

If using your existing backup directory instead, changing just the filename is
fine, but omit the mkdir commands: exclusive leaf creation intentionally fails
when that directory already exists. Never reuse an existing output filename;
--result-file can overwrite it. Preserve before-migration.sql unchanged.

### B3. Check the isolated restore destination and request approval

The test suite reported leaving taskcompass_integration empty, but recheck rather
than assuming it is still empty. Follow the milestone's identity, catalog and
effective privilege preflight for taskcompass_integration/taskcompass_test; inspect
tables/views, routines, triggers and events. No new accounts or grants should be
needed. Stop if the target has any existing objects or unexpected privileges.

Record the backup path/hash, source server version/platform, dataset UUID,
successful backup/verification exit codes and empty-target evidence. Review the
exact file and proposed import command with the owner before live restoration.
The earlier Stage 2A acceptance does not authorize this separate restore.

### B4. Restore only after the separate approval

The approved import reads after-gui.sql into taskcompass_integration using
taskcompass_test (not root). Use the exact import command in the milestone.
For the currently reviewed candidate, the concrete Linux command is below.
This is the separate restore approval checkpoint: review and explicitly approve
this exact file/target/import before running it. Keep TaskCompass closed. Confirm
the file still has SHA-256
f7aa21d4404cc706701cc358e2c2309dddc6c1854a0a7c522c4527a74bb4799c:

```bash
sha256sum /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql
```

Stop if the hash differs. After separate restore approval, run from the normal
Linux shell, not inside the MariaDB prompt:

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_test --password --default-character-set=utf8mb4 \
   taskcompass_integration \
   < /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/after-gui.sql
restoreExitCode=$?
printf 'Restore exit code: %s\n' "$restoreExitCode"
```

Enter the test-account password at the hidden prompt. No further terminal output
is normal on success; require exit 0 and then complete B5. Any nonzero exit or SQL
error means stop and preserve the partial state; do not rerun, reset or delete
tables. A partial import may already have committed DDL/data. This procedure does
not authorize cleanup, restoring development, or any account/privilege changes.

Do not run TaskCompassMigrate --apply on the destination first: the dump itself
creates the tables and restores migration metadata. Stop on any import error;
partial restoration requires inspection and separate recovery approval.

### B5. Verify the recovered database and compare its contents

Use TaskCompassMigrate --verify --integration with the explicit TEST environment
and test-account password. It must report schema 1 and the same dataset UUID.
Compare row counts and deterministically ordered exports of all five tables with
the still-quiescent source: Dataset, Task, CommandRecord, LocalChange and
SchemaMigration. Matching private export hashes, alongside schema verification,
prove that IDs, timestamps, task fields, tombstones and history were preserved.
An import exit code of zero alone is insufficient. Record results in the milestone.

For the current Linux rehearsal, use the following read-only comparison with
TaskCompass still closed. It writes private escaped batch exports beside the
backup, never to Git, and compares counts and every column of all five tables.
Each export uses its own restricted account and a hidden password prompt. A new
comparison directory prevents overwriting earlier evidence:

```bash
comparisonDirectory=$(umask 077; mktemp -d /home/justin/temp/TaskCompassBackups/Phase1.1-20261114-015008/compare-XXXXXX)
```

Stop if directory creation failed. Then run:

```bash
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
(
   umask 077
   mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
      --user=taskcompass_dev --password --default-character-set=utf8mb4 \
      --batch --skip-column-names --execute="$comparisonSql" taskcompass_dev \
      > "$comparisonDirectory/source.tsv" &&
   mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
      --user=taskcompass_test --password --default-character-set=utf8mb4 \
      --batch --skip-column-names --execute="$comparisonSql" taskcompass_integration \
      > "$comparisonDirectory/restored.tsv" &&
   sha256sum "$comparisonDirectory/source.tsv" "$comparisonDirectory/restored.tsv" &&
   cmp -s "$comparisonDirectory/source.tsv" "$comparisonDirectory/restored.tsv"
)
comparisonExitCode=$?
printf 'Comparison exit code: %s\n' "$comparisonExitCode"
```

The first password is the diagnostic account's; the second is the test account's.
Require both clients to succeed, two identical hashes and comparison exit 0.
Batch escaping is intentionally enabled (no --raw) so embedded newlines and NULL
values have deterministic representations. The first five export rows are table
counts; remaining rows contain private data, so share only hashes/status, not the
files. A mismatch requires investigation, not reimport or cleanup. This comparison
assumes all source/test writers remain stopped throughout; it does not lock out
concurrent writes itself.

### B6. Preserve the evidence

Do not run the GUI against the restored test database or rerun integration tests
there: those tests require an empty schema. Leave it intact; any cleanup is a
separate approval. Stage 2B success does not authorize Phase 1.2 or complete the
remaining platform-validation requirements.
