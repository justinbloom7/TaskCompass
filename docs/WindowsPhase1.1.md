<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.1 Windows validation: one script step at a time

Use **MSYS2 UCRT64** in the repository root. No hand-written SQL or environment
password exports are needed for this route. The runner is
[scripts/phase11-validate.sh](../scripts/phase11-validate.sh); no arguments prints
help and does not connect. Numbered entry scripts in scripts/phase11/ match the
01–11 stage headings below and sort in execution order. Each runs only its own
stage; none automatically advances to the next. All share the same runner and
safety checks. It does not install software or start services.
Keep the Windows MariaDB service running on TCP 127.0.0.1:3306.

This is a new Windows validation run, not a repeat of Linux validation. Windows
execution of the new runner is pending. The authoritative approval/result record
is [Phase1.1.md](milestones/Phase1.1.md). The script supports Linux TCP administration
too, but does not replace the existing Linux socket-admin instructions.

## Before starting

- Windows prerequisites remain in [Installation.md](Installation.md): UCRT64 GCC,
  CMake/Ninja, Qt Widgets/SQL/Test, QMYSQL connector dependencies, native MariaDB
  server/client/dump, and Bash tools. Windows PowerShell is used to restrict backup
  directory ACLs; no additional package is introduced.
- The earlier Windows setup should already have an empty taskcompass_dev database
  and SELECT-only taskcompass_dev@127.0.0.1 account. This runner never recreates or
  changes them. If missing, use the separately reviewed diagnostic setup first.
- Have passwords in a password manager: administrator root, diagnostic
  taskcompass_dev, test taskcompass_test, migration taskcompass_migrate and runtime
  taskcompass_app. The scripts label each prompt. New passwords are entered twice,
  not generated or saved behind the scenes.
- Open a fresh UCRT64 terminal. No need to source the older diagnostic environment.
  This runner sets its own Qt build paths and exact per-command account settings.
- `--approved` declares that the particular live step has already been approved;
  it is not permission to bypass the milestone approval process. Each write step
  also requires a typed confirmation. Linux approvals do not authorize Windows
  writes. Review the SQL first and obtain Windows approval before write steps.

Initialize these nonsecret shell variables once (repeat after opening a terminal):

```bash
dbOptions=()
```

Client/dump discovery is automatic. If it fails or picks a different installation,
set explicit paths matching your Windows MariaDB installation, for example:

```bash
dbOptions=(--client '/c/Program Files/MariaDB 13.0/bin/mariadb.exe'
           --dump '/c/Program Files/MariaDB 13.0/bin/mariadb-dump.exe')
```

Adjust those paths if your installation directory differs. Arrays preserve spaces.
All following commands are **Bash terminal commands**, not MariaDB prompt commands.
Do not run them as Windows Administrator merely to run TaskCompass.

## 01. Build and run ordinary tests

```bash
bash scripts/phase11/01-build.sh
```

It selects build-ucrt64, prints compiler/CMake versions, configures live CTest OFF,
builds, runs CTest and lists Qt SQL drivers. The live integration test should be
skipped here. Record the CTest totals and driver output; no GUI review is claimed.

## 02. Inspect this Windows server and review the SQL

```bash
bash scripts/phase11/02-inspect.sh "${dbOptions[@]}"
bash scripts/phase11/03-integration-setup.sh --print-plan
bash scripts/phase11/05-migration-setup.sh --print-plan
bash scripts/phase11/06-migrate.sh --print-plan
bash scripts/phase11/07-runtime-setup.sh --print-plan
```

Inspect prompts for root's database password. It reports only TaskCompass database
and account metadata plus server/platform identity. The plan commands have no
connections or prompts. SQL templates are for review: never put a real password
in them. Relevant accounts already existing is not a reason to reset them. Report
the inspection and choose the appropriate entry point before proceeding if the
Windows server already has Phase 1.1 tables/accounts.

If Windows already has a migrated development schema and final grants, skip fresh
migration-setup/migrate/runtime-setup after reviewing its state; go to gui. If the
test database/account already exist, skip integration-setup; integration-test will
validate grants/empty contents and stop on any mismatch. Never drop data to make
the sequence fit. Later steps enforce exact grants and reject inherited account
roles; PUBLIC is checked separately on MariaDB 10.11+.

## 03. Fresh isolated test setup, only if absent and approved

```bash
bash scripts/phase11/03-integration-setup.sh --approved "${dbOptions[@]}"
```

Uses the existing guarded provisioning helper. Confirmation:
`CREATE taskcompass_integration`. Prompts for root and a new test password twice.
Stops if either resource already exists; creates no application tables.

## 04. Approved integration test

```bash
bash scripts/phase11/04-integration-test.sh --approved "${dbOptions[@]}"
```

Confirmation: `TEST taskcompass_integration WITH DISPOSABLE RESET`.
Prompts for root to verify complete target emptiness, exact test grants, roles and
PUBLIC policy, then the test-account password. The disposable suite creates and
drops only its own integration tables. Require PASS/exit 0 and schema-left-empty
output. Stop on any failure; no automatic cleanup/repair is provided.

## 05. Back up empty development and provision its migration account

Choose a **new private directory outside the repository**, on a local Windows
filesystem supporting ACLs. It must not already exist; preserve older backups.

```bash
baselineDirectory="$HOME/TaskCompassBackups/windows-baseline-$(date +%Y%m%d-%H%M%S)"
bash scripts/phase11/05-migration-setup.sh --approved \
   --backup-dir "$baselineDirectory" "${dbOptions[@]}"
```

Confirmation: `PROVISION taskcompass_migrate`. Prompts: root, diagnostic password,
new migration password twice. Checks empty development/no new accounts, takes a
private before-migration.sql backup, captures its status/hash and only then creates
the fresh narrowly scoped migrator. Windows backup ACL is restricted to the
current user using PowerShell before any dump/export is written. If ACL/dump/
provisioning fails, stop and preserve the directory/evidence; don't retry over it.

## 06. Apply the approved migration

```bash
bash scripts/phase11/06-migrate.sh --approved \
   --backup-dir "$baselineDirectory" "${dbOptions[@]}"
```

Confirmation: `MIGRATE taskcompass_dev`. Prompts for root preflight and migrator
password. Rechecks baseline hash/server identity, empty schema and exact migration
grants before calling TaskCompassMigrate --apply and --verify. Require both success
messages with one dataset UUID. DDL can partially commit: on error stop; don't
rerun, reset, edit migration metadata, or broaden grants.

## 07. Provision runtime and revoke migrator writes

```bash
bash scripts/phase11/07-runtime-setup.sh --approved "${dbOptions[@]}"
```

Confirmation: `PROVISION taskcompass_app AND REVOKE migrator writes`. Prompts for
migrator verification, root and a new runtime password twice. Verifies exact
runtime/diagnostic/migrator final grants automatically. Existing runtime account
or partial provisioning stops the procedure rather than overwriting credentials.

## 08. GUI checklist and restart

```bash
bash scripts/phase11/08-gui.sh "${dbOptions[@]}"
```

Enter root's password for final privilege checks, then taskcompass_app's runtime
password. The runner verifies schema, then opens
the persistent GUI. Test create/edit, completion/show-completed/reopen, undo/redo
of each mutation, redo invalidation after a new edit, cancellation and validation
errors. Confirm sorting/filtering keeps selection stable. Close the GUI and run
the same command again: tasks/history persist and session undo clears. Record
each checklist result; the script cannot certify your interactive review. The
full checklist is in the milestone's **Stage 2A persistent GUI acceptance checklist**.

## 09. Close GUI and make a populated backup

```bash
snapshotDirectory="$HOME/TaskCompassBackups/windows-populated-$(date +%Y%m%d-%H%M%S)"
bash scripts/phase11/09-backup.sh \
   --backup-dir "$snapshotDirectory" "${dbOptions[@]}"
```

Confirmation: `ALL TASKCOMPASS WRITERS CLOSED`. Prompts: runtime verification,
then diagnostic password. Creates a private directory, exports all five tables
before/after the dump and rejects changes during the backup. Saves after-gui.sql,
checksums, dataset/server identity and a private source.tsv. **Do not publish the
dump or TSV files**: they include task notes/history. Keep TaskCompass closed until
comparison finishes; there is no server lock excluding noncooperating writers.

Privately inspect after-gui.sql: five expected tables/data; no DROP, account
changes, USE/CREATE DATABASE or unrelated objects. Keep the file unchanged.
Record the hash/path and obtain separate approval of that exact-file import.

## 10. Restore the approved file into the empty integration database

```bash
bash scripts/phase11/10-restore.sh --approved \
   --backup-dir "$snapshotDirectory" "${dbOptions[@]}"
```

The script checks both backup/source-export hashes. Type
`RESTORE taskcompass_integration <the printed SHA-256>`, then
`DUMP REVIEWED; WRITERS CLOSED`. It prompts for root to check empty target,
test grants/PUBLIC and matching backup server, then the test password and exact
authenticated identity. Only after those checks does it import into integration.
Require PASS/import exit 0. No --force, overwrite/reset or retries. An error may
leave partially committed DDL/data; stop for reviewed recovery.

## 11. Verify and compare automatically

```bash
bash scripts/phase11/11-compare.sh \
   --backup-dir "$snapshotDirectory" "${dbOptions[@]}"
```

Confirmation: `ALL TASKCOMPASS WRITERS CLOSED`. Prompts: test password, then
diagnostic password. Verifies schema/dataset, exports all five restored tables,
reads current development data, and compares both to the saved source snapshot.
Require three identical hashes and final PASS. Existing comparison files are not
overwritten. A mismatch stops without repair; preserve evidence.

After success, leave the restored integration database and backups intact.
**Do not run integration-test again**: its empty-target guard will reject this
restored schema. Cleanup is a separate approval, not part of this walkthrough.

## Report and limitations

Send nonsecret PASS/failure output, CTest totals, server/compiler/Qt versions,
dataset UUID, backup/export hashes and the GUI checklist results. The milestone
records actual evidence and remaining work; scripts do not edit that document or
mark the milestone complete. Windows validation remains pending until run there.

No credentials are written to SQL/config/log files or passed in arguments. Native
clients receive a per-child MYSQL_PWD environment value and the Qt executables use
their existing documented variables; they are cleared when the runner exits.
Other same-user processes may inspect environments; use a trusted local machine.
Client diagnostics are redacted. MariaDB 13's password-option/TLS warning is
explained when observed, without disabling TLS verification. No passwords are
requested for help/preview. TLS requirements for remote service use are outside
this loopback development workflow.

This runner conservatively rejects nonstandard grants, explicit account roles and
PUBLIC role inheritance for manual review. It does not silently weaken policy.
Backups/exports rely on Windows PowerShell ACL operations, whose actual behavior
still requires Windows validation. MariaDB DDL has no automatic transactional
rollback; failed provisioning/import/migration retains evidence. No database
cleanup, service management, dependency installation or later-phase work occurs.

Database-free helper tests (no live client):

```bash
bash -n scripts/phase11-validate.sh tests/Phase11ValidationScripts.test.sh
bash tests/Phase11ValidationScripts.test.sh
```
