<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7 — development upgrade and acceptance review package

**Proposed procedures, not permission to execute.** The authoritative approval and
result ledger is [Phase1.2.md](milestones/Phase1.2.md). Both isolated synthetic
suites passed, but no populated development snapshot, narrow-role rehearsal,
development upgrade or real organizational GUI acceptance has been performed.
Every numbered live stage below needs its own explicit platform/target approval.
From repository root, `sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256`
checks this runbook and the 19 proposed SQL files; it does not run SQL.
The first request is **B7.1 read-only preflight only**, on both specified hosts.

Preserve all Phase 1.1 reports, databases, restored taskcompass_integration instances
and private artifacts. Do not run their reset, migration-setup or restore scripts.
Do not start Phase 1.3. Do not install dependencies, change authentication, commit
or push. Owner runs interactive commands to keep passwords private.

## Stages, targets and approval boundaries

| Stage | Proposed operation / deliverable | Approval gate and success condition |
| --- | --- | --- |
| B7.1 | Fresh taskcompass_dev metadata/grant inspection and pinned schema-1 verification | Read-only on feledria/10.5.29 and feldria-w10/13.0.2 separately identified; known five-table schema, one dataset, reviewed effective privileges. No files/backups/DDL. |
| B7.2 | New private populated schema-1 dump, before/after exports and hashes per platform | Approve new external directory and quiescence; no overwrite of old artifacts, dump exit 0, exports identical. |
| B7.3 | Restore exact reviewed schema-1 dump into empty taskcompass_phase12_test | Separate exact-file/hash/target approval; both schema and every legacy value match source. Never run destructive synthetic suite on a populated clone. |
| B7.4a | Change only the rehearsal account to narrow migrator grants; upgrade clone to 2 | Exact grant file + unchanged 002 approval; verify schema 2 and zero legacy differences on both servers. |
| B7.4b | Change only rehearsal grants to runtime-equivalent; rollback-only DML probe | Explicit grant/DML approval; metadata exact, all probe rows rolled back, nine-table exports identical. |
| B7.5 | Fresh backup revalidation; temporary dev migrator grants and 001→002 | Platform-specific exact SQL/artifact approval only after both rehearsals; stop on any baseline drift or partial attempt. |
| B7.6 | Verify preservation; add runtime grants/revoke temporary migrator rights | Separate grant approval; schema 2/data preservation pass first; diagnostic stays unchanged. |
| B7.7 | Persistent organizational GUI, session history and restart acceptance | Approve deliberate QA data in dev; run checklist below on each platform under runtime account. |
| B7.8 | New private populated schema-2 backup and all-nine-table evidence | Separate backup/path/quiescence approval; exact nine-table data/DDL before/after hashes match. |
| B7.9 | New CLI-only taskcompass_phase12_restore account/database; exact schema-2 restore and comparison | Separate provisioning and exact-file restore approvals; preserve populated rehearsal and old restored databases. No application identity guard changes. |
| B7.10 | Review evidence and accept milestone | All actual results recorded; owner explicitly accepts. No automatic acceptance. |

Use independent approval/result records for Linux and Windows. Do not mix snapshots,
UUID expectations or hashes between servers. The source development dataset IDs
are expected to remain those accepted in Phase 1.1, but **fresh observations** decide
the baseline; do not reuse old counts or guess whether state is unchanged.

## Inspected infrastructure and limitations

- MigrationRunner supports explicit contiguous 1→2, unchanged 001, nine-statement
  002, latest fingerprint and attempt journal. Incomplete attempts stop; no resume.
- Existing CLI: `TaskCompassMigrate --verify --to-version 1|2`, `--apply --to-version 2`,
  `--phase12-test`. Default execution only prints 001; bare --apply targets 1.
- `DatabaseConfig` permits only dev app/migrator, original integration/test and
  Phase 1.2 test pairs. It **rejects taskcompass_dev diagnostic credentials for Qt
  migration verification**. Use diagnostic for native read/dump, existing readonly
  taskcompass_migrate for Qt schema-1 verification. Do not source dev-environment.sh
  for runtime/migration: it intentionally selects the diagnostic account.
- `TaskCompass` GUI accepts only taskcompass_app on taskcompass_dev and requires
  schema 2. It cannot run against a rehearsal clone. Keep this guard unchanged.
- Phase 1.1 09-backup is reusable **only for a verified schema-1 snapshot**, in a
  new private directory. It already provides Windows ACLs, no-overwrite, dump
  checksums and five-table before/after exports. Its filename after-gui.sql is just
  an artifact name; in this plan it is the new pre-upgrade schema-1 snapshot.
- Old 05/06 expect an empty dev schema and 07 expects absent accounts: do not use.
  Old 10/11 use the retained taskcompass_integration and five tables: do not use.
  Old 09/11 do not prove all-nine-table schema-2 backup/restore and must not be
  repurposed as if they did.
- Existing six-privilege phase12 test account can import a snapshot into its now
  empty target. Then exact approved grants are narrowed to rehearse migration and
  runtime SQL permissions. No new Qt identities, GUI bypass or generalized target
  selector is needed. These role changes have **not** been tested yet.
- A new CLI-only restore target preserves the populated rehearsal clone. Qt CLI/GUI
  deliberately cannot use it. Verification there is data/DDL equality against an
  already Qt-verified source plus FK/CHECK checks, not a falsely claimed Qt run.

## Common terminal definitions (no SQL until a function/command is invoked)

All commands are Bash from repository root (Linux terminal or UCRT64). Choose the
correct local tools once. The Windows paths below match the known installation;
stop if they differ and select the actual trusted installed client/dump pair.

```bash
set +x
taskCompassRepository=$(pwd -P)
b7Sql="$taskCompassRepository/scripts/phase12/b7"
if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}MYSQL_PWD;TASKCOMPASS_DB_PASSWORD;TASKCOMPASS_PHASE12_TEST_DB_PASSWORD"
   taskCompassClient='/c/Program Files/MariaDB 13.0/bin/mariadb.exe'
   taskCompassDump='/c/Program Files/MariaDB 13.0/bin/mariadb-dump.exe'
   taskCompassBuild=build-ucrt64
   taskCompassMigrate="$PWD/build-ucrt64/TaskCompassMigrate.exe"
   taskCompassGui="$PWD/build-ucrt64/TaskCompass.exe"
else
   taskCompassClient=$(command -v mariadb || command -v mysql)
   taskCompassDump=$(command -v mariadb-dump || command -v mysqldump)
   taskCompassBuild=build
   taskCompassMigrate="$PWD/build/TaskCompassMigrate"
   taskCompassGui="$PWD/build/TaskCompass"
fi
```

Read-only/admin SQL file invocation uses socket/sudo on Linux and native password
prompt on Windows. The function never captures or saves an administrator password;
SQL goes through stdin and --password prompts separately. Do not add --force.
On Linux refresh sudo authorization with `sudo -v` **after stage approval**, then
use -n so expiry fails instead of consuming SQL as a password. No full script is
run as root. Administrative operations are only those in the approved SQL file.

```bash
b7Admin() {
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
      env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
         --host=127.0.0.1 --port=3306 --user=root --password \
         --default-character-set=utf8mb4 --batch --raw "$@"
   else
      env -u MYSQL_PWD sudo -n -- "$taskCompassClient" --no-defaults \
         --protocol=SOCKET --user=root --default-character-set=utf8mb4 \
         --batch --raw "$@"
   fi
}
b7ReadDev() {
   env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user=taskcompass_dev --password \
      --default-character-set=utf8mb4 --batch --skip-column-names \
      --database=taskcompass_dev "$@"
}
b7ReadRehearsal() {
   env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_test --password \
      --default-character-set=utf8mb4 --batch --skip-column-names \
      --database=taskcompass_phase12_test "$@"
}
b7ReadRestore() {
   env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_restore --password \
      --default-character-set=utf8mb4 --batch --skip-column-names \
      --database=taskcompass_phase12_restore "$@"
}
```

File exports refuse an existing destination, and comparison uses already-required
sha256sum (no cmp/diffutils dependency). Invoke only after the relevant private
artifact/export approval and verified directory permissions:

```bash
b7Export() {
   local reader=$1 sqlFile=$2 outputFile=$3
   shift 3
   case "$reader" in b7ReadDev|b7ReadRehearsal|b7ReadRestore) ;; *) return 2 ;; esac
   [[ ! -e "$outputFile" && ! -L "$outputFile" ]] || return 1
   (set -o noclobber; "$reader" "$@" < "$sqlFile" > "$outputFile")
}
b7SameFiles() {
   local firstHash secondHash
   firstHash=$(sha256sum < "$1") || return 1
   secondHash=$(sha256sum < "$2") || return 1
   [[ "${firstHash%% *}" == "${secondHash%% *}" ]] || {
      printf 'STOP: compared files differ; preserve all evidence.\n' >&2
      return 1
   }
}
```

These read helper names describe their usage, not server-enforced readonly mode;
only pass the reviewed SELECT/SHOW export files to them. Imports/probes are separate
explicit commands below. Every command's exit must be checked immediately; stop
before the next command/stage on nonzero status. Read native-client errors locally
and redact SQL payload snippets/personal task data before sharing diagnostics.
Do not paste passwords or private exports into chat. Windows CLI warnings remain visible; never suppress by disabling TLS.

## B7.1 — read-only preflight (first approval requested)

Targets: feledria/10.5.29 and feldria-w10/13.0.2, each local port 3306. Stop all
TaskCompass clients, old binaries, test processes and SQL writers; do not stop the
MariaDB service. The scoped process report helps but cannot prove that an unselected
DB session or noncooperating administrator will not write. Require owner quiescence.

Linux after approval (refresh sudo first); Windows after its approval includes
PUBLIC report because MariaDB 13 supports it. Do not run PUBLIC query on 10.5.

```bash
# Linux only, after B7.1 approval:
sudo -v
cat "$b7Sql/preflight-dev.sql" "$b7Sql/preflight-dev-grants.sql" |
   b7Admin --database=taskcompass_dev
```

```bash
# Windows UCRT64 only, after B7.1 approval:
cat "$b7Sql/preflight-dev.sql" "$b7Sql/preflight-dev-grants.sql" \
   "$b7Sql/public-grants.sql" | b7Admin --database=taskcompass_dev
```

No SELECT of task titles/notes, authentication hashes, unrelated database rows or
full PROCESSLIST query text occurs. The scoped account grant metadata may reveal
an unexpected privilege scope so it can be rejected, not used. Expected:

- Approved server/version/port, selected database taskcompass_dev, charset/collation
  utf8mb4/utf8mb4_unicode_ci. No other target sessions/writers.
- Exactly SchemaMigration/Dataset/Task/CommandRecord/LocalChange, all BASE TABLE,
  InnoDB (Windows may normalize table-name case). No views/routines/triggers/events.
- Exactly migration row 1 with checksum
  `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
  Record its full tuple and the dataset UUID; one canonical Dataset only.
- Record **all** row counts, including completed/tombstoned tasks and immutable
  history/outbox. No assumption of zero or active-only counts. If no task/history
  exists, report that before claiming a populated rehearsal; do not seed dev
  automatically to satisfy that criterion.
- Account host only 127.0.0.1; direct global privileges only USAGE, no GRANT OPTION,
  no explicit roles or unexpected column grants. Migrator and diagnostic each only
  schema SELECT. Runtime has the existing 9 table-privilege rows below. No schema
  runtime DML or DDL. Unexpected/other scopes stop for review, not automatic repair.
- PUBLIC grants are separate from direct rights. Ignore unrelated test/test_%
  scopes; reject any PUBLIC write/DDL/grant authority matching dev or planned clone
  targets (SQL wildcard rules, including escaped underscores). PUBLIC roles or
  unrecognized/global administrative grants stop for review. Do not change PUBLIC.
  Existing validate-grants.sh can inform review but is not a universal role validator.

| Schema-1 runtime table | Direct privileges |
| --- | --- |
| Task | SELECT, INSERT, UPDATE |
| CommandRecord, LocalChange | SELECT, INSERT |
| Dataset, SchemaMigration | SELECT |

Now verify the **actual current DDL fingerprint** with the existing readonly
migrator; a stored checksum alone is insufficient. Same command both platforms:

```bash
(
   set +x
   export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
   export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_migrate
   export TASKCOMPASS_DB_ISOLATED=1
   IFS= read -r -s -p 'Existing readonly taskcompass_migrate password: ' TASKCOMPASS_DB_PASSWORD
   printf '\n'
   export TASKCOMPASS_DB_PASSWORD
   "$taskCompassMigrate" --verify --to-version 1
   b7VerifyExit=$?
   unset TASKCOMPASS_DB_PASSWORD
   exit "$b7VerifyExit"
)
```

Expected `Schema 1 verified; dataset <same UUID>`, exit 0. Connection setup issues
session-only UTF-8/UTC/strict/check settings; it does not modify data or apply DDL.
No grant is added for verification. Stop for missing accounts, incompatible schema,
checksum/fingerprint/version mismatch, unexpected objects or conflicting identity.
Do not rewrite history/fingerprints, run --apply, broaden grants or reset a database.
First approval does not include backup creation, file exports, restores or writes.

## B7.2 — fresh populated schema-1 backup and evidence

Approval identifies platform, **new private external directory**, quiescence and
exact commands. Close writers and pin --verify --to-version 1 immediately before
backup. Preserve all old Phase 1.1 backups/exports. Do not reuse an existing folder.

```bash
# Assignments create nothing; retain this variable throughout B7.
if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
   b7Schema1Directory="$HOME/TaskCompassBackups/Phase1.2-schema1-windows-$(date +%Y%m%d-%H%M%S)"
else
   b7Schema1Directory="$HOME/temp/TaskCompassBackups/Phase1.2-schema1-linux-$(date +%Y%m%d-%H%M%S)"
fi
```

After explicit B7.2 approval, use the inspected existing schema-1 helper:

```bash
scripts/phase11/09-backup.sh --backup-dir "$b7Schema1Directory" \
   --client "$taskCompassClient" --dump "$taskCompassDump" --build-dir "$taskCompassBuild"
```

Confirm ALL TASKCOMPASS WRITERS CLOSED; password prompts are existing runtime then
SELECT-only diagnostic. This helper creates the directory with umask 077 and, on
Windows, restricts ACL to the current user's SID/FullControl before files are made.
If ACL cannot be established/verified, stop without dumping. Dump options are:

```text
--no-defaults --protocol=TCP --host=127.0.0.1 --port=3306
--user=taskcompass_dev --default-character-set=utf8mb4 --single-transaction
--skip-lock-tables --no-tablespaces --skip-triggers --skip-add-drop-table
--skip-add-locks --skip-disable-keys --no-create-db --hex-blob
--result-file=<new-private-directory>/after-gui.sql taskcompass_dev
```

This is a single database, no --databases/--all-databases. No trigger/routine/event
contents are omitted silently: preflight must establish there are none. No locks,
admin dump credentials, DROP/USE/database/user/grant DDL or --force is requested.
Require helper exit 0, nonempty dump, its SHA-256 check and unchanged source.tsv /
source-after.tsv. It creates server.txt/dataset.txt and checksums. Re-run pinned
schema-1 verification; no upgrade is permitted during backup. Preserve failed files
and mark unusable; never overwrite/retry into the directory.

Make stronger private canonical exports immediately after helper success while all
writers remain closed. Here legacy-before means **before migration**, not before
the dump: the helper already recorded pre/post five-table source equality around
its dump. Compare the two canonical captures too, and later compare the restored
dump to them. Do not claim these separate exports are one server transaction.
Stop on drift; an approved replacement capture uses a new directory. Commands:

```bash
b7Export b7ReadDev "$b7Sql/legacy-export.sql" "$b7Schema1Directory/legacy-before.hex.tsv"
b7Export b7ReadDev "$b7Sql/ddl1-export.sql" "$b7Schema1Directory/schema1.ddl.txt" --raw
b7Export b7ReadDev "$b7Sql/legacy-export.sql" "$b7Schema1Directory/legacy-after.hex.tsv"
b7SameFiles "$b7Schema1Directory/legacy-before.hex.tsv" \
   "$b7Schema1Directory/legacy-after.hex.tsv"
(cd -- "$b7Schema1Directory" && sha256sum --check after-gui.sql.sha256 source.tsv.sha256)
```

Require both canonical export hashes equal and all client exits 0. Add checksums
for the canonical/DDL evidence; these are **private task data** despite HEX:

```bash
(cd -- "$b7Schema1Directory" && set -o noclobber && sha256sum legacy-before.hex.tsv \
   legacy-after.hex.tsv schema1.ddl.txt > b7-schema1-evidence.sha256)
```

The runtime-verification UUID, helper dataset.txt, fresh preflight UUID and dump
Dataset UUID must agree. Locally inspect the entire dump using less; do not paste
it into review. Require only the known five table definitions, their indexes/FKs/
CHECKs, INSERTs and normal **session-only** client prologue/epilogue. No source DB
qualifiers redirecting writes, stored code, DEFINER, GLOBAL settings, OUTFILE,
accounts/grants, DROP/REPLACE/TRUNCATE or USE/CREATE DATABASE. MariaDB dumps commonly
restore with session FK checks disabled, then restored; this is allowed **only for
the exact reviewed import session**, never for migration/runtime. It does not prove
existing rows satisfy FKs, so post-import equality and health checks are required.

Record dump/client/server versions, exit 0, nonempty check, path/hash, dataset ID
and counts. Windows ACL verification must precede exports too. All exports stay
private outside Git. Client 13 sandbox headers may be incompatible with older
clients; use the same-platform/version pair. No cross-server/version restore or
editing the dump to change its hash is authorized. Exact artifact hash must be
reviewed before later import approval; a hash alone is not SQL safety review.

## B7.3 — populated schema-1 recovery/rehearsal clone

Use the **now-empty taskcompass_phase12_test**, with a separate approval for the
exact local after-gui.sql hash, target, existing test account and same-server import.
This preserves restored taskcompass_integration. It is not the destructive suite:
**do not run 04-integration-test.sh on the clone**. It must remain retained afterward.

After B7.3 approval, inspect target and its original six direct grants/PUBLIC:

```bash
cat "$b7Sql/preflight-rehearsal.sql" "$b7Sql/preflight-clone-grants.sql" |
   b7Admin --database=taskcompass_phase12_test
# Windows 13 also:
b7Admin < "$b7Sql/public-grants.sql"
```

Require existing utf8mb4 target, no tables/views/stored code, no other sessions,
known account@127.0.0.1 with exact SELECT/INSERT/UPDATE/CREATE/DROP/ALTER on this
schema only. No unexpected global/table/column/role/PUBLIC rights. Require dump
hash and all backup evidence hashes match and no qualified source writes in dump.
Root only inspects; restore uses the dedicated account with rights to this target:

```bash
(cd -- "$b7Schema1Directory" && sha256sum --check after-gui.sql.sha256 b7-schema1-evidence.sha256)
env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
   --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_test --password \
   --default-character-set=utf8mb4 --database=taskcompass_phase12_test \
   < "$b7Schema1Directory/after-gui.sql"
restoreExit=$?
printf 'Schema-1 clone import exit: %s\n' "$restoreExit"
```

Require 0. Failure leaves partial clone for review, no retry/reset. DDL is not
transactionally undone. Do not reinitialize, applyInitial, create a new Dataset
or edit SchemaMigration fingerprints. Compare original UUID/row 1/counts plus:

```bash
b7Export b7ReadRehearsal "$b7Sql/legacy-export.sql" "$b7Schema1Directory/rehearsal1.hex.tsv"
b7Export b7ReadRehearsal "$b7Sql/ddl1-export.sql" "$b7Schema1Directory/rehearsal1.ddl.txt" --raw
```

Use --raw for both DDL exports as shown; no artifact is overwritten.
Canonical HEX exports use the default escaped batch mode; values are ASCII and
carry explicit NULL/type tags. Compare hashes: legacy-before.hex.tsv=rehearsal1.hex.tsv
and schema1.ddl.txt=rehearsal1.ddl.txt. Source remains quiescent. Exact DDL equality
includes column defaults, PK/UK/FK/CHECK/index/engine/charset definitions, not just
names. Reject cross-schema FK references to taskcompass_dev in restored definitions.

Verify restored schema with the real Qt runner and dedicated phase12 config:

```bash
(
   set +x
   export TASKCOMPASS_PHASE12_TEST_DB_HOST=127.0.0.1 TASKCOMPASS_PHASE12_TEST_DB_PORT=3306
   export TASKCOMPASS_PHASE12_TEST_DB_NAME=taskcompass_phase12_test
   export TASKCOMPASS_PHASE12_TEST_DB_USER=taskcompass_phase12_test TASKCOMPASS_PHASE12_TEST_DB_ISOLATED=1
   IFS= read -r -s -p 'Existing phase12 test-account password: ' TASKCOMPASS_PHASE12_TEST_DB_PASSWORD
   printf '\n'
   export TASKCOMPASS_PHASE12_TEST_DB_PASSWORD
   "$taskCompassMigrate" --verify --phase12-test --to-version 1
   verifyCloneExit=$?
   unset TASKCOMPASS_PHASE12_TEST_DB_PASSWORD
   exit "$verifyCloneExit"
)
```

Expected schema 1/same dataset/exit 0; checks include actual fingerprint and FK
schema endpoints. This proves restore on the local server. The clone is a passive
rehearsal copy with the source Dataset UUID; never enroll it as a syncing client.

## B7.4 — minimum-privilege migration/runtime rehearsal on both servers

### B7.4a migration role

Approve only this existing test account's grant change, pinned 002 apply and
verification for the populated clone. First assert exact original six grants again.
The file [rehearsal-migration-grants.sql](../scripts/phase12/b7/rehearsal-migration-grants.sql)
revokes those six schema grants, grants schema SELECT and exactly the eight
per-table grants used by the proposed dev migrator. No password/account change.

```bash
b7Admin < "$b7Sql/rehearsal-migration-grants.sql"
b7Admin < "$b7Sql/preflight-clone-grants.sql"
```

Require only schema SELECT plus the grant matrix below. Reuse the subshell in B7.3
but replace its single Qt invocation by these **existing supported** commands:

```bash
"$taskCompassMigrate" --verify --phase12-test --to-version 1 &&
"$taskCompassMigrate" --apply --phase12-test --to-version 2 &&
"$taskCompassMigrate" --verify --phase12-test --to-version 2
```

Stop on any denial; record safe step/native code and preserve partial attempts.
Do not grant extra rights to push through. A failed privilege swap can leave partial
grants; inspect first, request concrete recovery. Completed2 is idempotent in the
runner but any interrupted or uncertain operation still needs explicit review.
Before changing roles, prove clone upgrade preservation:

```bash
b7Export b7ReadRehearsal "$b7Sql/legacy-export.sql" "$b7Schema1Directory/rehearsal2-legacy.hex.tsv"
b7SameFiles "$b7Schema1Directory/legacy-before.hex.tsv" "$b7Schema1Directory/rehearsal2-legacy.hex.tsv"
b7ReadRehearsal < "$b7Sql/upgrade-postconditions.sql"
b7ReadRehearsal < "$b7Sql/schema2-health.sql"
b7Export b7ReadRehearsal "$b7Sql/ddl2-export.sql" "$b7Schema1Directory/rehearsal2.ddl.txt" --raw
```

All postconditions in B7.5 apply, including unchanged metadata row 1 and completed
attempt2. This is not permission to proceed when any comparison/count fails.

### Proposed temporary migrator matrix (dev and rehearsal)

| Scope | Rights |
| --- | --- |
| Exact schema | SELECT (existing on dev) |
| Pillar, Project, Objective | CREATE on each |
| MigrationAttempt | CREATE, INSERT, UPDATE |
| Task, CommandRecord, LocalChange | ALTER on each |
| SchemaMigration | INSERT (new row 2 only in reviewed code) |

Plan a maintenance window and adequate free storage for ALTER table rebuild/copies;
large histories may take longer than the synthetic fixtures. No duration or disk
headroom has been measured on the actual populated databases.

Table-level ALTER necessarily permits more than this migration (such as column
changes); use only the approved runner during maintenance, then revoke it.
No DROP, Dataset INSERT/UPDATE, domain INSERT/UPDATE, global privileges, INDEX,
GRANT OPTION or metadata UPDATE is proposed beyond MigrationAttempt. No standalone
CREATE INDEX or rename occurs; index creation is inside CREATE/ALTER. Verify absent
new-table grant support and exact minimums in these real rehearsals before dev.
There is no claim that Stage 1B's broader test grants proved these narrower sets.

### B7.4b runtime role

After clone2 preservation passes, separately approve
[rehearsal-runtime-grants.sql](../scripts/phase12/b7/rehearsal-runtime-grants.sql).
It removes all migration rights including schema SELECT and grants the exact
runtime table set: **19 individual table privileges**, no schema-wide rights.

| Schema-2 runtime table | Rights |
| --- | --- |
| Task, Pillar, Project, Objective | SELECT, INSERT, UPDATE |
| CommandRecord, LocalChange | SELECT, INSERT |
| Dataset, SchemaMigration, MigrationAttempt | SELECT |

```bash
b7Admin < "$b7Sql/rehearsal-runtime-grants.sql"
b7Admin < "$b7Sql/preflight-clone-grants.sql"
```

Require exact direct metadata, no roles/extra columns/grant option/PUBLIC authority.
Capture the private probe baseline:

```bash
b7Export b7ReadRehearsal "$b7Sql/schema2-export.sql" "$b7Schema1Directory/rehearsal-before-probe.hex.tsv"
```

Use B7.3's phase12 environment to run `--verify --phase12-test --to-version 2` with this account;
its SELECT-only metadata access must suffice. Then the separately approved probe:

```bash
env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
   --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_test --password \
   --default-character-set=utf8mb4 --database=taskcompass_phase12_test \
   < "$b7Sql/runtime-probe.sql"
```

Expected four UPDATE counts 1 and six probe-row counts 0 after ROLLBACK. On any
client error stop; without --force, disconnect rolls back that new InnoDB DML
transaction. No DDL/grants or COMMIT is in the probe. Snapshot all nine tables again and require complete equality:

```bash
b7Export b7ReadRehearsal "$b7Sql/schema2-export.sql" "$b7Schema1Directory/rehearsal-after-probe.hex.tsv"
b7SameFiles "$b7Schema1Directory/rehearsal-before-probe.hex.tsv" "$b7Schema1Directory/rehearsal-after-probe.hex.tsv"
```

This validates SQL permission
coverage, not a real GUI/CommandService acceptance test. The account still has the
test identity, so **do not** use --apply as a runtime identity guard test there;
the CLI intentionally recognizes that identity as a test/migrator. For the real dev
app identity, its preconnection --apply rejection is already offline-tested and
will be repeated after B7.6. Do not perform CREATE/DROP/DELETE denial probes.

The normal GUI cannot target this clone; do not broaden its allowlist. Real GUI
acceptance waits for separately approved dev upgrade. Retain the populated clone
and its evidence; never run destructive integration/reset helpers on it afterward.

## B7.5 — exact development migration approval and preservation

Only after both populated clone/narrow-role rehearsals succeed, approve the
specific dev host, observed schema/dataset, baseline dump hash and exact grant/002
files. Repeat B7.1 with all writers closed. Compare a fresh legacy export with the
sealed baseline. If any data/DDL/account/version/fingerprint changed, stop; obtain
a fresh backup/rehearsal or reviewed replacement plan, not a stale approval.

The temporary grant delta is exactly
[migration-dev-grants.sql](../scripts/phase12/b7/migration-dev-grants.sql):

```sql
GRANT CREATE ON taskcompass_dev.Pillar TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE ON taskcompass_dev.Project TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE ON taskcompass_dev.Objective TO 'taskcompass_migrate'@'127.0.0.1';
GRANT CREATE, INSERT, UPDATE ON taskcompass_dev.MigrationAttempt TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.Task TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.CommandRecord TO 'taskcompass_migrate'@'127.0.0.1';
GRANT ALTER ON taskcompass_dev.LocalChange TO 'taskcompass_migrate'@'127.0.0.1';
GRANT INSERT ON taskcompass_dev.SchemaMigration TO 'taskcompass_migrate'@'127.0.0.1';
```

No CREATE USER/password/authentication changes. Existing schema SELECT remains.
After its explicit approval:

```bash
b7Admin < "$b7Sql/migration-dev-grants.sql"
b7Admin < "$b7Sql/preflight-dev-grants.sql"
```

Require that exact delta and no unexpected effective rights. Grant failure is
not atomic: stop and inspect partial grants, no repeated GRANT/REVOKE repair.
Now use B7.1's migrator/password subshell and replace its Qt invocation with:

```bash
"$taskCompassMigrate" --verify --to-version 1 &&
"$taskCompassMigrate" --apply --to-version 2 &&
"$taskCompassMigrate" --verify --to-version 2
```

This uses taskcompass_migrate, never root/runtime/diagnostic; no --integration or
--phase12-test. Exit 0/schema2/same Dataset UUID required. Do not use bare --apply
or execute 002 manually: that bypasses the journal/checksum/preservation protocol.
The exact actual DDL is [002](../migrations/mariadb/002_OrganizationalManagement.sql),
nine statements, SHA-256 `7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`.
001 stays `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
No preflight result has yet established readiness to run this on either dev DB.

### Mandatory immediate comparison (also applies to B7.4a clone upgrade)

Before granting runtime access/opening GUI, repeat the root metadata inventory
(preflight-dev.sql; now expect nine tables, rows1/2 and no stored code), then
export all **legacy columns** using
legacy-export.sql, and DDL using ddl2-export.sql. Dev commands (use b7ReadRehearsal
and rehearsal2 filenames on the clone):

```bash
b7Export b7ReadDev "$b7Sql/legacy-export.sql" "$b7Schema1Directory/dev2-legacy.hex.tsv"
b7SameFiles "$b7Schema1Directory/legacy-before.hex.tsv" "$b7Schema1Directory/dev2-legacy.hex.tsv"
b7ReadDev < "$b7Sql/upgrade-postconditions.sql"
b7ReadDev < "$b7Sql/schema2-health.sql"
b7Export b7ReadDev "$b7Sql/ddl2-export.sql" "$b7Schema1Directory/dev2.ddl.txt" --raw
```

Each mismatch/nonzero command or violation count stops acceptance. Both platforms
compare **within their own** baseline; never compare Linux/Windows fingerprints.
Expected:

- Nine known InnoDB tables; no stored code. SchemaMigration rows exactly 1 and 2;
  row 1's complete tuple unchanged, row 2 checksum equals exact embedded 002 and
  latest fingerprint matches Qt verification.
- Exactly one MigrationAttempt row, version2/checksum002/baselineFingerprint=row1's
  original fingerprint, state completed, lastCompletedStep=9, sane started/completed
  timestamps. Qt verifies these; do not rewrite them to force a pass.
- Same Dataset UUID/name/createdAt and count1; all existing Task rows/tombstones,
  timestamps/revisions/completion state unchanged. New task owner columns all NULL.
- All old CommandRecord/LocalChange values unchanged, including byte-for-byte
  beforeSnapshot/afterSnapshot/immutablePayload, operation IDs, compensation links,
  payloadVersion=1 and pending acknowledgment state. New typed targets all NULL,
  legacy taskId remains set. No metadata operation creates a domain command/change.
- Pillar/Project/Objective empty before runtime use. Every health count zero.

legacy-export.sql selects these exact columns, with the original stable sort keys:

| Table | Included original columns (none omitted) |
| --- | --- |
| Dataset | id, name, createdAt |
| Task | id, datasetId, title, notes, lifecycleStatus, createdAt, updatedAt, completedAt, deletedAt, localRevision |
| CommandRecord | id, operationId, datasetId, taskId, commandType, payloadVersion, recordedAt, beforeSnapshot, afterSnapshot, resultingRevision, compensatesCommandId, reappliesCommandId |
| LocalChange | id, operationId, datasetId, taskId, localRevision, payloadVersion, immutablePayload, createdAt, acknowledgmentState |
| SchemaMigration | version, checksum, schemaFingerprint, appliedAt, **only version=1** |

Strings are H+HEX (empty string H), NULL is N, integers I+decimal. Thus NULL/empty,
Unicode, tabs/newlines/backslashes/NUL, JSON formatting/property order and payload
bytes are compared exactly without parsing/reserializing history. Task sort key id;
other domain/history tables id, schema metadata version. Count rows include all
statuses/tombstones; no active-only filter. Whole-table SELECT * after adding fields
is invalid for the schema1→2 comparison. After GUI mutations, use a new all-nine-table
snapshot; do not falsely compare the changed application to the old frozen baseline.

## B7.6 — runtime grants and migrator write revocation

Only after preservation passes, separately approve the exact
[runtime-dev-grants-and-revoke.sql](../scripts/phase12/b7/runtime-dev-grants-and-revoke.sql).
It adds SELECT/INSERT/UPDATE on each Pillar/Project/Objective and SELECT on
MigrationAttempt to the existing app account, then revokes the eight temporary
migrator per-table grants listed above. No password reset or new account.

```bash
b7Admin < "$b7Sql/runtime-dev-grants-and-revoke.sql"
b7Admin < "$b7Sql/preflight-dev-grants.sql"
# Windows 13 also recheck PUBLIC:
b7Admin < "$b7Sql/public-grants.sql"
```

Final state: migrator and diagnostic each only direct schema SELECT (unchanged
passwords); app exactly the 19 table privileges in B7.4b, no schema/global DML/DDL,
no roles/extra columns/grant option. Diagnostic retains original SELECT-only grant;
it automatically includes the new tables. No UPDATE/DELETE on audit/outbox.
Failure can leave partial grants: no GUI until exact final metadata is reviewed.
Use the dev Qt subshell with USER=taskcompass_app and --verify --to-version 2;
expect schema2 and same UUID under these permissions. Also verify migration account
can still read/verify2 with SELECT only. Runtime guard test requires no real password
and must fail **before connecting**:

```bash
env TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306 \
   TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_app \
   TASKCOMPASS_DB_ISOLATED=1 TASKCOMPASS_DB_PASSWORD=runtime-guard-placeholder \
   "$taskCompassMigrate" --apply --to-version 2
```

Expected exit1/dedicated migration-account rejection; not a CREATE/DROP denial probe.
No broad test role is used for the development GUI.

## B7.7 — persistent GUI acceptance (both platforms)

Approve identifiable disposable **B7 QA** entities/tasks in taskcompass_dev. Do not
edit/delete actual pre-existing owner work just to exercise controls. Preserve its
IDs/history and the schema-1 snapshot. No SQL reset after QA. Runtime launch:

```bash
(
   set +x
   export TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306
   export TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_app
   export TASKCOMPASS_DB_ISOLATED=1
   IFS= read -r -s -p 'Existing taskcompass_app runtime password: ' TASKCOMPASS_DB_PASSWORD
   printf '\n'
   export TASKCOMPASS_DB_PASSWORD
   "$taskCompassGui"
   guiExit=$?
   unset TASKCOMPASS_DB_PASSWORD
   exit "$guiExit"
)
```

Repeat this whole invocation for restart, never the old Phase 1.1 gui wrapper.
Record Linux and Windows separately, PASS/FAIL plus observed IDs/row counts and
relevant errors. Screenshots/private notes must exclude credentials/personal work.

- [ ] Startup on schema2 with runtime grants succeeds; all legacy active work is
  visible in All Tasks/Independent, completed hidden by default and recoverable by
  Show Completed. Existing task UUIDs/lifecycle remain unchanged by upgrade.
- [ ] Create root pillar, nested subpillars, project under pillar/subpillar, nested
  projects, objective under project and directly under pillar. Names/notes edit and
  rename persist; optional description NULL/empty behavior is retained. At least
  five mixed levels are practical to inspect interactively; automated 2001-level
  pillar/2002-level project core and 2001-level model tests remain separate evidence.
- [ ] Objective cannot own another objective. Projects/objectives require one legal
  pillar/project owner; task owner choices are Independent, Direct Project or one
  Objective. No task-to-pillar or redundant objective-plus-project fields appear.
- [ ] Global New Task remains independent while a project/objective is selected;
  context New Task supplies that legal owner. Editing a task between all three
  ownership forms clears incompatible references atomically and preserves UUID.
  A pillar selection cannot directly own a task. Cancel creates/changes nothing.
- [ ] Project-oriented and objective-oriented views show the **same** records/IDs;
  duplicate names have path/type/UUID disambiguation. Selection and derived task
  paths refresh after ancestor-only edits. Filters do not change stored ownership.
- [ ] Move pillar/project subtree and flat objective. Picker excludes self/descendant/
  illegal parents; no cycle can be saved. Preview includes hidden/completed/archived
  descendant work, not just filtered rows. UUIDs, child revisions/explicit flags,
  tasks, completion/audit records persist; only root parent/revision changes.
- [ ] Archive parent with children/tasks. Default active lists hide inherited work;
  Show Archived exposes it with explicit/inherited labels. Explicitly archive a child
  too; restoring the parent leaves that child archived. Restoring a child under an
  archived ancestor leaves it effectively archived; Locate Archived Ancestor works.
  No automatic ancestor/descendant flag rewrite. Creation under archived owner is
  blocked; confirmed moves into archive warn; moving out can remove inherited state.
- [ ] One chronological Undo/Redo spans create/edit/rename/move/archive/restore,
  task owner edit and task completion/reopening. Business values are restored while
  revisions/updatedAt advance and durable compensation receipts remain. A new
  successful mutation invalidates redo. Undo-create refuses nonempty organization
  with live dependents (including completed tasks); no user Delete/purge controls.
- [ ] With two deliberate QA GUI sessions, edit stale work in B then attempt a
  conflicting edit/inverse in A. Failure must not overwrite B or advance history;
  reload explicitly. No blind retry. Restore connectivity only through normal
  service availability, not DB resets. Do not deliberately kill the server in QA.
- [ ] Existing continuing task/recurrence/work-progress features remain deferred;
  no context ordering, priorities, due/scheduling, planning or mobile/sync controls
  are claimed implemented by this milestone. Existing one-time completion/reopen
  and text/search/sort/keyboard/dialog/table/splitter behavior remains usable.
- [ ] Close normally (wait for worker completion), relaunch with runtime credentials.
  Organization/task values/owners/archive state and selected/expanded UUIDs/view
  preferences persist. Session undo/redo starts empty; audit/history remains.
  Switching views does not create duplicate objectives or lose prior task data.

Any failure pauses acceptance. Database history and unknown-commit handling require
inspection; no manual UPDATE to make data/UI appear correct. No milestone accepted
merely because GUI opens or isolated tests passed.

## B7.8 — fresh schema-2 backup and complete evidence

After GUI acceptance, close **all** writers and approve a new populated schema-2
private snapshot on each platform. Do not reuse old folders or the schema-1 dump.
Do not use Phase 1.1 09-backup for schema2: it exports only five tables.

Create a new private external directory with the function below (reviewed copy of
the existing Phase 1.1 no-overwrite/Windows ACL procedure). This definition itself
creates nothing; invocation is part of backup approval. The base parent must be
outside the repository and on an ACL-capable filesystem for Windows. Stop if that
cannot be guaranteed/verified.

```bash
b7NewPrivateDirectory() {
   (
      set -e
      umask 077
      mkdir -p -- "$(dirname -- "$b7Schema2Directory")"
      b7Parent=$(cd -- "$(dirname -- "$b7Schema2Directory")" && pwd -P)
      [[ "$b7Parent/" != "$taskCompassRepository/"* ]] || exit 1
      mkdir -- "$b7Schema2Directory"
      if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
         TASKCOMPASS_PRIVATE_DIRECTORY=$(cygpath -w "$b7Schema2Directory") \
         powershell.exe -NoProfile -NonInteractive -Command '
            $ErrorActionPreference="Stop"
            $sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User
            $acl=New-Object System.Security.AccessControl.DirectorySecurity
            $acl.SetOwner($sid)
            $acl.SetAccessRuleProtection($true,$false)
            $rule=New-Object System.Security.AccessControl.FileSystemAccessRule($sid,"FullControl","ContainerInherit,ObjectInherit","None","Allow")
            $acl.AddAccessRule($rule)
            Set-Acl -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY -AclObject $acl
            $actual=Get-Acl -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY
            $rules=$actual.GetAccessRules($true,$true,[System.Security.Principal.SecurityIdentifier])
            if (!$actual.AreAccessRulesProtected -or $rules.Count -ne 1 -or
                $rules[0].IdentityReference.Value -ne $sid.Value -or
                $rules[0].FileSystemRights -ne [System.Security.AccessControl.FileSystemRights]::FullControl -or
                $rules[0].AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) {
               throw "Private backup ACL verification failed"
            }
         '
      fi
   )
}
```

No files are written before ACL success. If directory already exists or any step
fails, stop. Do not repair its ACL silently or continue dumping into it. An empty
failed-attempt directory is preserved. Do not execute chmod/chown on old evidence.
After approval choose a NEW, separately named schema-2 path. Do not overwrite the
b7Schema1Directory variable; the two original snapshots remain distinguishable.

```bash
if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
   b7Schema2Directory="$HOME/TaskCompassBackups/Phase1.2-schema2-windows-$(date +%Y%m%d-%H%M%S)"
else
   b7Schema2Directory="$HOME/temp/TaskCompassBackups/Phase1.2-schema2-linux-$(date +%Y%m%d-%H%M%S)"
fi
b7NewPrivateDirectory
```

Verify Qt schema2 under runtime privileges before capture; then capture complete
state, canonical rows and raw DDL before the dump:

```bash
b7Admin --database=taskcompass_dev < "$b7Sql/preflight-dev-grants.sql"
b7Export b7ReadDev "$b7Sql/schema2-export.sql" "$b7Schema2Directory/source2-before.hex.tsv"
b7Export b7ReadDev "$b7Sql/ddl2-export.sql" "$b7Schema2Directory/source2-before.ddl.txt" --raw
```

Explicit dump command (same flags/SELECT-only account as inspected helper; no
persistent credential file). On Windows normalize only the output path for the
native client. Its password prompt is the **diagnostic** taskcompass_dev password.

```bash
taskCompassDumpFile="$b7Schema2Directory/schema2.sql"
if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
   taskCompassDumpFile=$(cygpath -m "$taskCompassDumpFile")
fi
[[ ! -e "$b7Schema2Directory/schema2.sql" && ! -L "$b7Schema2Directory/schema2.sql" ]] &&
env -u MYSQL_PWD "$taskCompassDump" --no-defaults --protocol=TCP \
   --host=127.0.0.1 --port=3306 --user=taskcompass_dev --password \
   --default-character-set=utf8mb4 --single-transaction --skip-lock-tables \
   --no-tablespaces --skip-triggers --skip-add-drop-table --skip-add-locks \
   --skip-disable-keys --no-create-db --hex-blob \
   --result-file="$taskCompassDumpFile" taskcompass_dev
schema2DumpExit=$?
printf 'Schema-2 backup exit: %s\n' "$schema2DumpExit"
```

Require exit0 and nonempty schema2.sql. Check absence of an existing destination
before invoking the native dumper; it does not inherit Bash noclobber protection
for its own --result-file. Use an entirely new directory and never repeat this
command to the same path, even after failure.

```bash
test -s "$b7Schema2Directory/schema2.sql"
b7Export b7ReadDev "$b7Sql/schema2-export.sql" "$b7Schema2Directory/source2-after.hex.tsv"
b7Export b7ReadDev "$b7Sql/ddl2-export.sql" "$b7Schema2Directory/source2-after.ddl.txt" --raw
b7SameFiles "$b7Schema2Directory/source2-before.hex.tsv" "$b7Schema2Directory/source2-after.hex.tsv"
b7SameFiles "$b7Schema2Directory/source2-before.ddl.txt" "$b7Schema2Directory/source2-after.ddl.txt"
(cd -- "$b7Schema2Directory" && set -o noclobber && sha256sum schema2.sql source2-before.hex.tsv \
   source2-after.hex.tsv source2-before.ddl.txt source2-after.ddl.txt > schema2-evidence.sha256)
```

Seal schema2.sql only after private SQL safety review as in B7.2 (now exactly nine
tables, both payload versions and the attempt journal); record exact artifact hash,
server/client versions, Dataset UUID, schema rows/attempt tuple, table counts and
runtime grants. Run schema2-health.sql on the coherent source, all counts zero.
No unobserved source mutation/DDL may occur during capture. Mismatch stops sealing;
preserve all files and request review, not overwrite or fix hashes.

## B7.9 — isolated schema-2 restore, preserve populated rehearsal

The original taskcompass_integration stays untouched; the populated
taskcompass_phase12_test rehearsal stays retained too. Propose a fresh **CLI-only**
taskcompass_phase12_restore / same-named account@127.0.0.1. Separate provisioning
approval and exact-file restore approval are required. This target is intentionally
not in DatabaseConfig/GUI allowlists. Do not add it or spoof taskcompass_dev.

First inspect under admin **without** --database, because the new database must
not exist yet:

```bash
cat "$b7Sql/preflight-restore.sql" "$b7Sql/preflight-clone-grants.sql" | b7Admin
```

Review the reported server and require absent database and no same-name account on any host before provisioning.
The exact [restore-account.template.sql](../scripts/phase12/b7/restore-account.template.sql)
creates the new utf8mb4 database/account with **SELECT, INSERT, CREATE only** on its
escaped exact schema. No DROP/ALTER/UPDATE/global/grant option. These suffice for
an approved plain-table no-DROP/no-LOCK/no-disable-keys dump; if real tooling asks
for more, stop/review rather than silently broaden rights.

Provisioning password token must be rendered only in memory. A source-only exact
operator recipe follows; **do not replace the token in a saved file**. It reads the
new password from the terminal, not SQL stdin, escapes quotes under NO_BACKSLASH_ESCAPES
and redacts both literal forms from errors. Refresh Linux sudo authorization before
this operation, after provisioning approval. Native Windows root --password prompt
remains separate from the new restore-user password.

```bash
(
   set +x
   IFS= read -r -s -p 'New restore-account password (12+ characters): ' b7RestorePassword
   printf '\n'
   [[ ${#b7RestorePassword} -ge 12 ]] || exit 1
   IFS= read -r -s -p 'Repeat restore-account password: ' b7RestoreRepeat
   printf '\n'
   [[ "$b7RestorePassword" == "$b7RestoreRepeat" ]] || exit 1
   b7Literal="'${b7RestorePassword//\'/\'\'}'"
   b7Template=$(cat "$b7Sql/restore-account.template.sql")
   printf '%s\n' "${b7Template/'{{PASSWORD_SQL_LITERAL}}'/"$b7Literal"}" |
      b7Admin 2> >(while IFS= read -r b7Line || [[ -n "$b7Line" ]]; do
         b7Line=${b7Line//"${b7RestorePassword//\'/\'\'}"/<redacted>}
         printf '%s\n' "${b7Line//"$b7RestorePassword"/<redacted>}" >&2
      done)
   provisionExit=$?
   unset b7RestorePassword b7RestoreRepeat b7Literal b7Template
   exit "$provisionExit"
)
```

Validate new account's exact three grants/no roles/PUBLIC and entirely empty target
before importing. Approval binds the specific same-server schema2.sql SHA-256, local
client version and directory evidence. Do not combine CREATE with import or migrate
this target. It contains the completed schema2 snapshot including history/attempt.

```bash
(cd -- "$b7Schema2Directory" && sha256sum --check schema2-evidence.sha256)
env -u MYSQL_PWD "$taskCompassClient" --no-defaults --protocol=TCP \
   --host=127.0.0.1 --port=3306 --user=taskcompass_phase12_restore --password \
   --default-character-set=utf8mb4 --database=taskcompass_phase12_restore \
   < "$b7Schema2Directory/schema2.sql"
schema2RestoreExit=$?
printf 'Schema-2 restore exit: %s\n' "$schema2RestoreExit"
```

Require exit0; on any error preserve partial objects, no rerun/reset. Dump session
FK-check disabling is not global and ends with the client; post-import checks are
mandatory because reenabling does not retroactively validate rows.

```bash
b7Export b7ReadRestore "$b7Sql/schema2-export.sql" "$b7Schema2Directory/restored2.hex.tsv"
b7Export b7ReadRestore "$b7Sql/ddl2-export.sql" "$b7Schema2Directory/restored2.ddl.txt" --raw
b7SameFiles "$b7Schema2Directory/source2-before.hex.tsv" "$b7Schema2Directory/restored2.hex.tsv"
b7SameFiles "$b7Schema2Directory/source2-before.ddl.txt" "$b7Schema2Directory/restored2.ddl.txt"
b7ReadRestore < "$b7Sql/schema2-health.sql"
b7Admin --database=taskcompass_phase12_restore < "$b7Sql/preflight-restore.sql"
```

Require exactly nine BASE TABLE/InnoDB objects, no stored code/extra sessions,
expected encoding, all 85 columns/every row/count including MigrationAttempt and
both SchemaMigration rows byte-for-byte equal, DDL equality (all constraints/indexes),
and all FK/CHECK health counts zero. Referenced schemas must be this restore target;
this approved read-only check must return zero:

```sql
SELECT COUNT(*) AS crossSchemaReferences FROM information_schema.KEY_COLUMN_USAGE
   WHERE TABLE_SCHEMA='taskcompass_phase12_restore'
     AND REFERENCED_TABLE_NAME IS NOT NULL
     AND REFERENCED_TABLE_SCHEMA<>'taskcompass_phase12_restore';
```

Source Dataset UUID remains intentionally identical; restore isn't a new initialized dataset.

Because Qt source verification already matched the schema-2 fingerprint and the
same-server source/restore DDL bytes match, this is a transitive schema-fingerprint
proof without weakening Qt identity guards. It is **not** a TaskCompassMigrate or
GUI run on this unsupported target. Do not edit fingerprint rows for portability.
Read-only schema2-export of current dev should still match the sealed before file;
if dev intentionally resumed editing, report that separately and retain snapshot
comparison evidence, never reset source to force equality.

After successful restore validation, request separate approval for
[restore-account-readonly.sql](../scripts/phase12/b7/restore-account-readonly.sql),
which revokes only INSERT and CREATE, leaving SELECT. Verify metadata afterward.
Preserve populated restore/rehearsal databases and all private evidence; no cleanup
is included in acceptance. No sync/device enrollment/mobile or new runtime target.

## B7.10 — final evidence review (no automatic acceptance)

For each platform record the hostname/server/client/Qt/build versions, exact approved
SQL/file hashes, preflight identities/grants, schema1 backup and populated-clone
comparisons, narrow-role results, dev upgrade/legacy preservation, final runtime
rights, GUI checklist, schema2 backup and restore equality. Keep private paths/hashes
and nonsecret summaries in the milestone; task exports/dumps stay outside Git.
Separate owner-reported outcomes from agent-executed checks and pending checks.
Any incomplete comparison, unreviewed failure, missing platform or GUI/restart check
blocks final acceptance. Request explicit owner acceptance only after all criteria
pass. Retain every database and backup; acceptance authorizes no cleanup or Phase 1.3.

## Failure matrix, recovery and rollback limitations

| Failure point | Required action |
| --- | --- |
| B7.1 unknown account/schema/version/fingerprint/grant | Stop before backup/grant/DDL. Record discrepancy; no automatic repair. |
| ACL, dump, export, hash or SQL review failure | Preserve unusable artifacts/exit codes; no import/upgrade. New capture needs a new directory/approval if scope changes. |
| Populated clone import failure | Preserve partial clone. No synthetic suite, re-import, DROP or restore retry until actual state and exact recovery SQL are reviewed. |
| Grant/revoke failure | Grants may be partially applied/nontransactional. Keep all clients closed, inspect exact effective grants, propose only observed deltas for fresh approval. |
| Before 002 first CREATE | Version1 may be intact; verify/read-only before concluding. Never assume no side effect solely from an error message. |
| Journal bootstrap / any of nine DDL statements | Retain all objects, SchemaMigration and MigrationAttempt if present. Journal step can lag actual DDL. No suffix execution/resume/repair. |
| All DDL but missing row2 / incomplete attempt | Runtime stays blocked; metadata must not be fabricated. Preserve state, request recovery. |
| Final metadata commit outcome uncertain | Read-only verify/catalog/history review; if complete, accept only after explicit reviewed preservation checks. Never blindly replay. |
| Post-migration legacy comparison fails | Stop before runtime grants/GUI. Preserve both source and baseline private evidence; no UPDATE to reconcile or overwrite backup. |
| Runtime/GUI/undo conflict or error | Preserve history and state, reload/inspect. Do not broaden grants or manually update tasks/history. |
| Schema2 backup/restore mismatch | Preserve both exports/DDL/dumps and failed restore; no source reset or hash rewriting. |

There is **no generic down migration**. MariaDB DDL is not a transactionally
reversible bundle. For a failed dev upgrade, preserve a reviewed forensic snapshot
of actual partial state first (requires its own explicit artifact/backup approval).
Rehearse the verified schema1 snapshot into a **fresh approved recovery target**;
never restore over partial dev, retained clones or original integration by default.
B7.3 already demonstrated the pre-upgrade snapshot restore on the local server.
A fresh CLI-only restore target can demonstrate schema1 recovery via five-table
legacy/DDL equality; if the proposed name is already occupied, stop and request a
new exact name/SQL approval. Do not broaden application identity guards for it.

Actual in-place recovery of taskcompass_dev would destroy current contents and is
**not preauthorized here**. Its DROP/restore SQL, exact reviewed backup hash,
remaining schema/attempt/data, account-grant rollback and stopped-client state must
be approved separately after inspection. No recovery file can safely be finalized
before that unknown failure state exists. If schema2 GUI edits already occurred,
preserve them first; blindly restoring schema1 would discard new work. A valid
backup is not proof of restore until the isolated comparison succeeds. The current
GUI requires schema2; do not launch it on a recovered schema1 or change metadata to
make it run. Retain an independently managed accepted Phase 1.1 build for any
approved schema1 fallback; no git reset/checkout is performed by this plan.

## References and review limits

Primary-source cross-checks: [MariaDB ALTER TABLE privileges](https://mariadb.com/docs/server/reference/sql-statements/data-definition/alter/alter-table),
[GRANT/index rights](https://mariadb.com/docs/server/reference/sql-statements/account-management-sql-statements/grant),
[mariadb-dump and sandbox/transaction limits](https://mariadb.com/docs/server/clients-and-utilities/backup-restore-and-import-clients/mariadb-dump).
These support the proposed least-privilege and same-platform capture policy; actual
narrow-role, populated restore, GUI and schema2 recovery results are still pending.
The diagrams continue deriving from unchanged migration SQL, not these procedures.
