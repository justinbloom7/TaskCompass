<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.5 — Development upgrade operator package

**Linux and Windows B7.5 passed (owner-reported, 2026-11-17), including final
privileges, original preservation and health verification. Preserve all evidence;
do not rerun mutation/completion stages. No agent live execution.** Approval/results are recorded in the
[milestone](milestones/Phase1.2.md). Both platforms passed B7.1–B7.4b according
to owner reports; these rehearsal prerequisites are satisfied. Fresh development
checks below must still pass. No completed rehearsal is rerun or modified.

This guide supplements the pinned [B7 review package](Phase1.2B7.md). Its original
pending statements are historical. The current owner request includes the final
runtime transition previously labeled B7.6; it is prepared here as **stage04**, a
separate operator invocation to be included explicitly in platform authorization.
GUI acceptance, backups/restores, reset/recovery and Phase1.3 are excluded.

## Execution order and first approval checkpoint

First approve **Linux only**, all five stages listed below on feledria/MariaDB10.5.29.
After Linux results are reviewed, request independent approval for Windows on
feldria-w10/MariaDB13.0.2. A Linux approval never authorizes Windows. `--approved`
records invocation intent; `--platform` must match the actual terminal. Neither flag
replaces the recorded platform/stage authorization. No typed confirmation phrase
or redundant yes/no prompt is used. Only passwords are requested.

After Linux approval, with all TaskCompass writers stopped, run from repository
root, **one command at a time**, stopping on any error:

```bash
scripts/phase12/b75/01-preflight.sh --approved --platform linux
scripts/phase12/b75/02-migration-grants.sh --approved --platform linux
scripts/phase12/b75/03-migrate.sh --approved --platform linux
scripts/phase12/b75/04-runtime-grants.sh --approved --platform linux
scripts/phase12/b75/05-verify.sh --approved --platform linux
```

**Do not run Windows until separately approved after Linux review.** Then use the
MSYS2 UCRT64 terminal and the same five commands with `--platform windows`:

```bash
scripts/phase12/b75/01-preflight.sh --approved --platform windows
scripts/phase12/b75/02-migration-grants.sh --approved --platform windows
scripts/phase12/b75/03-migrate.sh --approved --platform windows
scripts/phase12/b75/04-runtime-grants.sh --approved --platform windows
scripts/phase12/b75/05-verify.sh --approved --platform windows
```

`--help` or `--print-plan` prints guidance without connecting or changing files.
Optional trusted `--client`, `--build-dir` and `--backup-dir` paths follow the
existing B7 conventions; there is no arbitrary database/account/UUID/hash override.
Do not use old Phase1.1 empty-database, backup, reset or restore scripts here.

## Fixed development targets and preserved snapshots

Both use `taskcompass_dev`, TCP127.0.0.1:3306. Separate existing accounts are
`taskcompass_migrate` for migrations/Qt verification, `taskcompass_dev` for native
read-only comparisons and `taskcompass_app` for final runtime verification.
Passwords differ between platforms; use the account named in the prompt. Checked
passwords stay in memory for that run, with three authentication-only attempts.
Operations are never automatically retried. Administrator passwords are not
captured: Linux uses sudo/socket; Windows uses the native root password prompt.
TLS configuration is unchanged.

| Platform | Server / version | Dataset UUID | Approved original row counts |
| --- | --- | --- | --- |
| Linux | feledria /10.5.29-MariaDB | `21be9da7-b767-4578-b972-cc9a35122ddb` | Dataset1, Task4, CommandRecord28, LocalChange28, SchemaMigration1 |
| Windows | feldria-w10 /13.0.2-MariaDB | `457bb52b-fea4-4347-9c47-675c6b035c90` | Dataset1, Task2, CommandRecord16, LocalChange16, SchemaMigration1 |

Private source directories remain:

- Linux: `/home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703`.
- Windows: `$HOME/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917`.

Pinned `after-gui.sql` SHA256 values:

- Linux: `83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26`.
- Windows: `6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50`.

Original private ACL/mode, checksum manifests, native client identity, B7.3 import/
comparison receipts, B7.4a preservation evidence and B7.4b runtime/probe evidence
must all validate before a live development connection. Both B7.4b nine-table
exports must equal their grant baseline and retained probe counts must be correct.
These are file checks only; no preserved rehearsal database is queried or changed.
The owner's prior private dump review remains the reviewed-file authority.

If the development baseline changed, stop: a new snapshot/rehearsal or specifically
reviewed replacement plan is needed. Never update a checksum or expectation merely
to force a pass. Existing original backups are revalidated, not overwritten or
restored. Source001, source002 and built runner previews must still match:

- 001: `bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0`.
- 002: `7de311935843e4fc04cbca74d025d9f38986a4154d70bdd29b35a2820f87b736`.

## Stages and exact SQL transitions

### 01 — Read-only preflight

Verify schema1/UUID/checksum/registered fingerprint using the existing migrator.
Compare all original columns, count rows and raw schema1 DDL against that platform's sealed
B7.2 files. Root metadata must match server, port, database, table-case mode,
utf8mb4/collation, enabled CHECK/FK validation and five expected InnoDB tables.
Reject stored routines/triggers/events and any other selected-development-schema
session or session under any of the three development accounts, including idle
sessions with no selected database. Verify all account hosts and complete privilege
matrices, no explicit roles, unexpected global/column rights or grant option.
Windows PUBLIC grants must not add effective development rights; unrelated
`test` and `test\_%` grants remain acceptable.

Expected PASS: fresh dev/schema1/backup/rehearsal/account baseline. No new evidence
file or database mutation. Stop writers before every stage; checks are observations,
not a firewall against a newly started client or an unrelated privileged administrator.
The migration runner also uses its migration lock and shared mutation lease.

### 02 — Temporary migration privileges

Revalidate stage01 conditions, including after administrator authentication. Capture
original data and redacted-authentication-free grant metadata privately. Apply only
[migration-dev-grants.sql](../scripts/phase12/b7/migration-dev-grants.sql), SHA256
`517aaef4966eb3a50f0ec6f0c6f1b66f540ba474043ff54a46f5c4ed8030d4fb`:

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

Migrator retains existing schema SELECT and receives ten individual table rights;
no global privileges, DROP, GRANT OPTION or domain-data write grant. Diagnostic
schema SELECT and application's original nine table rights remain unchanged.
Verify exact metadata and unchanged original data before success. No migration yet.

### 03 — Apply002 and prove preservation

Require sealed stage02 receipts; freshly verify unchanged schema1 and exact
migration privileges. Write attempt marker **before** a single invocation:

```text
TaskCompassMigrate --apply --to-version 2
```

Only `TASKCOMPASS_DB_*` configuration for taskcompass_migrate/taskcompass_dev is
used, never root, a test target or manual SQL replay. Root is metadata-only.
Independently run `--verify --to-version 2` and require original UUID. Validate
nine tables, rows1/2, exact002 checksum/fingerprint and completed journal with
lastCompletedStep9 and sane timestamps. Full original metadata row1, UUID/name/
creation time, all Task rows including tombstones/completed tasks and all historical
command/change fields must match the sealed schema1 export byte-for-byte.
No JSON reserialization is used. New task owner and typed audit references stay
NULL, original payload versions stay1, all new organizational tables are empty.
All59 CHECK/FK health results must be present and zero. Store complete schema2
data, DDL, legacy export, postconditions and health evidence privately and seal it.

Expected PASS: dev schema2/UUID/journal and original data/history preservation.
Application access is not yet expanded. No GUI launch.

### 04 — Runtime privileges and migrator revocation (former B7.6 transition)

Require successful stage03 seals and fresh schema2/full-data equality. Check runtime
credentials before mutation, then apply only
[runtime-dev-grants-and-revoke.sql](../scripts/phase12/b7/runtime-dev-grants-and-revoke.sql),
SHA256 `c474535e2d9af43ba570f6d0694936a59d01e6e2b8bab37a779f5cce10e04442`:

```sql
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Pillar TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Project TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT, INSERT, UPDATE ON taskcompass_dev.Objective TO 'taskcompass_app'@'127.0.0.1';
GRANT SELECT ON taskcompass_dev.MigrationAttempt TO 'taskcompass_app'@'127.0.0.1';
REVOKE CREATE ON taskcompass_dev.Pillar FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE ON taskcompass_dev.Project FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE ON taskcompass_dev.Objective FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE CREATE, INSERT, UPDATE ON taskcompass_dev.MigrationAttempt FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE ALTER ON taskcompass_dev.Task FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE ALTER ON taskcompass_dev.CommandRecord FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE ALTER ON taskcompass_dev.LocalChange FROM 'taskcompass_migrate'@'127.0.0.1';
REVOKE INSERT ON taskcompass_dev.SchemaMigration FROM 'taskcompass_migrate'@'127.0.0.1';
```

Expected exact final matrix:

| Account / scope | Final privileges |
| --- | --- |
| taskcompass_dev / exact schema | SELECT only, unchanged |
| taskcompass_migrate / exact schema | SELECT only; all temporary table rights removed |
| taskcompass_app / Task, Pillar, Project, Objective | SELECT, INSERT, UPDATE on each |
| taskcompass_app / CommandRecord, LocalChange | SELECT, INSERT on each |
| taskcompass_app / Dataset, SchemaMigration, MigrationAttempt | SELECT on each |

The application has19 individual table rights and no schema-wide rights. No
UPDATE/DELETE on history/outbox, account creation or password/plugin changes.
Require Qt verification under both application and SELECT-only migration identities,
unchanged diagnostic metadata and all-nine-table equality before success.

### 05 — Independent final verification

Recheck all prerequisite and development seals, schema2/UUID, original data/DDL,
complete final privilege metadata and both Qt identities. Verify the runtime CLI
`--apply --to-version 2` rejects **before connecting**, using a literal placeholder
password rather than an actual credential. Export full schema2 data and all health
rules again; require equality with stage03 and zero violations. Seal final results.
Expected final PASS: development upgrade/final privileges/preservation/health.
Do not open GUI until later acceptance is authorized.

## Current Linux preservation failure: read-only diagnosis

Owner reported stage01/02 passes and successful schema2 runner/UUID verification in
stage03, followed by a combined legacy/DDL comparison failure. Stage03 has **not**
passed preservation; do not rerun it or proceed to stage04/05. No completion marker
should be added by hand. The old error did not identify which comparison failed.
The new version names the SQL/export and expected/current hashes.

Run this diagnostic only on the reported Linux failure state, with writers stopped:

```bash
scripts/phase12/b75/06-inspect-failure.sh --approved --platform linux
```

It validates existing seals/grant receipts/attempt marker, independently verifies
schema2 and exact migration privileges, and captures SELECT/SHOW CREATE results in
a fresh private `b75-inspection-<timestamp>-<pid>` child directory. It compares all
original data and schema2 DDL separately; it prints hashes and a DDL-only diff if
`diff` is installed (otherwise reports private paths for comparison). It never
prints task/history values, replays SQL or writes a completion receipt. Session SET
statements only control SHOW CREATE rendering, not schema/data. No extra software
installation is required.

Send the diagnostic output for review. A DDL difference is not automatically
normalized or accepted as harmless. The inspection preserves old files/attempt
markers and does not authorize grants, repair, resume, restore, Windows operations
or GUI. No migration retry is needed to gather these facts.

## Reviewed Linux index variation and verification-only completion

**Current status:** owner reported successful verification-only completion and
stage04 runtime transition/stage05 final verification on 2026-11-17. Linux B7.5 is
verified; do not rerun its mutation/completion stages. Windows development stages01–05 are approved for owner execution, with results
pending; GUI acceptance remains separately gated. The original
failure and reviewed completion procedure below are retained as history.

The owner-reported inspection found all original data byte-identical (legacy SHA
`87e134d0fe5a55c47dc6123e50cdeb5fa3ce12e4b14466ac2ac80ba61b2e551c`).
The only DDL difference is the implicit LocalChange FK-supporting index:
rehearsal `changeTask`, development `changeTaskV2`; both plain nonunique indexes on
exactly `(datasetId,taskId)`, with its displayed line in a different position.
No DDL/grant/data repair or migration rerun is needed to address this comparison.

The [comparison helper](../scripts/phase12/b75/compare-schema2-ddl.sh) now permits
only this exact alias/line-position variation inside LocalChange. It requires one
recognized supporting-index definition per export and preserves every other DDL
byte. Different columns/order, uniqueness, unknown names, duplicate indexes,
constraint/type/engine changes or other line reordering still reject. It is an
additional comparison, not a substitute for Qt registered shape/FK/index checks.
Raw DDL exports and migration fingerprints remain untouched; fingerprints are not
rewritten to make different physical renderings equal. Schema1 and all legacy
columns/history still compare byte-for-byte without this exception.

For the reported already-completed Linux runner state, the next operator command
is verification-only, with writers closed:

```bash
scripts/phase12/b75/07-complete-reviewed-verification.sh --approved --platform linux
```

It requires the retained migration attempt, sealed grants and no prior success/
preservation outputs. It verifies schema2/completed journal/UUID, exact narrow
migration privileges, original legacy data, narrowly equivalent DDL, empty new
owners/tables, all59 health results0 and full exports. Only after all checks pass
does it write new completion/preservation receipts and an additional reviewed-
completion seal. It never executes --apply, grants, DML, schema repair, reset or
restore; it never alters/deletes the original attempt or any inspection file.
Partial/failed journals and unrelated differences refuse completion.

Report its result before stage04. Do not rerun stage03. This Linux-only completion
is not a Windows approval or GUI acceptance. Any unexpected result still stops
for review, without automatically resuming or repairing anything.

## Private evidence and failures

New `b75-*` files/manifests are added beside the original sealed capture. No existing
file is overwritten. Each mutation has an attempt marker before execution and a
success receipt only after verification. Receipts bind platform, development
target, snapshot and reviewed SQL digests. Stage02 seals baseline/grant reports;
stage03 seals legacy/DDL/postconditions/health/full-schema exports; stage04 seals
final grant metadata; stage05 seals independent final data/health results.

On any error, interruption, denial, unexpected object/grant/data, nonzero health
count, missing/incomplete journal or uncertain commit: **stop immediately** and
preserve actual database state and all artifacts. Do not delete markers, rerun
mutation stages, manually replay SQL, reset, repair, resume, broaden privileges,
restore or change checksum expectations. Report the stage and safe error output.

MariaDB DDL and grants can persist partially despite failure. Script receipts and
the runner journal deliberately prevent automatic resume. The schema1 backup is
an immutable recovery source, not an automatic rollback. Recovery requires a
specific decision based on read-only inspection of the actual partial state;
restoration must use a separately approved isolated target first. No generic
in-place DROP/restore/down-migration procedure is authorized. Preserve originals
and rehearsals even when a new development operation fails.

## Offline tests

Linux commands used for preparation:

```bash
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -E '^(MariaDbIntegrationTests|OrganizationIntegrationTests)$'
```

On Windows use `build-ucrt64`. New focused test:

```bash
ctest --test-dir build -R '^Phase12DevelopmentUpgradeScripts$' --output-on-failure
```

All operator tests use fake clients/private fixture files only. Neither preserved
rehearsal nor development database is used by ordinary tests. Native Windows and
live development results remain pending, distinct from mocked branch validation.
