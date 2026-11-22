<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.8 — Fresh populated schema-2 backup and complete evidence

**Prepared for review; no live stage executed by the agent. Linux stage02 approval
is requested first. Stages03/04 and Windows require their own authorization.**
The [milestone](milestones/Phase1.2.md) records actual checks and decisions;
[Linux instructions](Phase1.2B78Linux.md) and
[Windows instructions](Phase1.2B78Windows.md) give numbered commands. This package
supersedes the paused draft, preserving its failed mock test report.

B7.7 GUI functional acceptance is recorded for both platforms. Linux's changed
protected task and missing restart comparison remain **UNRESOLVED**. A fresh backup
preserves the current state; it does not repair, rebaseline or waive those exceptions.
No B7.9 restore/provisioning, B7.10 acceptance, Phase1.3 or cleanup is included.

## Fixed targets, accounts and prerequisites

| Platform | Server | Fixed target | Dataset UUID |
| --- | --- | --- | --- |
| Linux / CentOS Stream9 | feledria, MariaDB10.5.29, TCP3306 | taskcompass_dev | 21be9da7-b767-4578-b972-cc9a35122ddb |
| Windows / UCRT64 | feldria-w10, MariaDB13.0.2, TCP3306 | taskcompass_dev | 457bb52b-fea4-4347-9c47-675c6b035c90 |

All GUI clients and other TaskCompass/database writers must be closed throughout
preflight/capture. Automatic admin metadata checks reject other sessions, unexpected
objects/stored code/engines, identity changes and privileges outside the reviewed
matrix. Point-in-time session checks cannot prevent a new external writer from
connecting later: stopped writers remain a required operator precondition.

- taskcompass_dev: diagnostic schema SELECT only; all exports/dump use this account.
- taskcompass_migrate: schema SELECT only, used solely for schema verification.
- taskcompass_app: exact19 runtime table privileges, used solely for verification.
- Linux admin: existing sudo + local Unix socket root, read-only metadata queries.
- Windows admin: native MariaDB root TCP password prompt, read-only metadata queries.

No account, password, plugin, privilege or TLS-verification changes. Non-admin
passwords are identity-checked and reused in process memory only; up to three
retries apply only to confirmed authentication rejection on the read-only probe.
The existing read-only admin client connection is reused for capture's before/after
checks, then closed: no repeated root-password entry within that capture. Connection
loss stops instead of reconnecting. No operation/connection-loss/capture retry.
Passwords are never put in files or
command-line arguments. Use platform-specific passwords.

B7.2/B7.3/B7.4/B7.5 sealed artifacts and pinned review/migration hashes are verified
without replaying those stages. B7.7 initial data must match B7.5. Windows requires
all five sealed checkpoints and exact before/after restart equality; current data
must match its final capture. Linux requires the retained failed final export and
current data to match it exactly. That file remains **unsealed historical evidence**,
not a successful checkpoint. Only its already documented protected Task
24dd11c3-59df-4e23-ac21-422bcf2a1e3a may differ from B7.5 in updatedAt,
localRevision and objectiveId; any missing original row or additional protected
change stops preparation/capture. Historical metadata/audit/outbox originals remain
exact. The exception is printed and carried into new evidence, never marked passed.

Default prior evidence is the already recorded B7.2 capture and QA directories;
no path re-entry is needed. Trusted --client/--dump/--build-dir/--python overrides
exist for offline mocks/tool paths, not host/database/user overrides. --evidence-dir
and --qa-dir can select existing reviewed evidence inside that capture; stop and
review a changed source/path rather than substituting another baseline.

## Numbered stages and separate authorization

1. **01-check.sh --platform …**: offline only. Checks pinned operator/SQL files,
   required installed tools/native client/dumper versions and embedded migrations;
   emits the shared environment report. No login or private database export.
2. **02-preflight.sh --approved --platform …**: first live, read-only stage. Checks
   source/evidence, native dumper version against B7.2, schema2 checksum/fingerprint/
   completed migration journal under both verifier accounts, exact nine InnoDB
   tables, UUID/server/charset, final privileges/PUBLIC policy and absent writers.
   All current CHECK/FK health counts must be zero. Streams the complete canonical
   nine-table export only to SHA256 and compares against retained B7.7 final data;
   it creates no backup directory or database export file. The shared environment
   report uses explicitly labeled saved B7.5 server metadata, whose identity agrees
   with the fresh admin check. Build notes are ignored local metadata, not task data.
3. **03-capture.sh --approved --platform …**: separate approval AFTER preflight
   review. Repeats fresh checks; creates a NEW private external timestamp/PID
   directory, captures before/after data/raw DDL/health/grants, performs exactly one
   native nine-table dump, independently re-verifies source and compares all values.
   It records dump exit status, metadata and SHA256 capture manifest. This is
   **CAPTURE COMPLETE, PRIVATE REVIEW PENDING**, not sealed reviewed evidence.
4. **04-review.sh --approved --platform … --snapshot-dir PATH --dump-reviewed**:
   offline, after the owner has privately read the entire SQL file. Rechecks private
   permissions/ACLs, capture checksums/receipt/historical references/health/raw DDL,
   data equality and dump contents; records the explicit flag attestation and seals
   schema2-evidence.sha256. No typed phrases and no database connection. The flag
   is required operator review evidence; it is not supplied automatically by stage03.

No script launches the GUI or calls the Phase1.1 five-table backup procedure.
Source-only library reuse never invokes a migration, grant or reset stage.

## Destination and evidence policy

Capture defaults:

- Linux: `$HOME/temp/TaskCompassBackups/Phase1.2-schema2-linux-YYYYmmdd-HHMMSS-PID`.
- Windows: `$HOME/TaskCompassBackups/Phase1.2-schema2-windows-YYYYmmdd-HHMMSS-PID`.

The physical parent must be outside the repository (including symlink/junction
resolution) and outside earlier sealed capture directories. A trusted --parent
may change the external base only with the reviewed approval scope. No caller may
supply an existing capture destination. mkdir/no-clobber guards prevent overwrites;
Windows ACL creation/verification occurs on the new directory before artifact writes.
Linux directories/files must be current-user700/600; Windows directory ACL is
protected, owner-only FullControl, inherited by files and checked individually.
Failed empty directories/partial artifacts remain for review; no ACL repair or cleanup.

Exactly nine tables: SchemaMigration, Dataset, Task, CommandRecord, LocalChange,
MigrationAttempt, Pillar, Project, Objective. The existing schema2-export.sql uses
stable column/row ordering, NULL/integer/HEX encoding and counts, preserving every
text byte including raw historical JSON whitespace. Native LF/CRLF record endings
are retained and accepted by offline parsing; byte comparisons remain exact within
the platform. Raw DDL is never normalized in the backup. The existing verifier and
reviewed DDL comparator check source structure against B7.5, accepting only its
previously reviewed identical LocalChange index alias variation. The two fresh
raw DDL exports themselves must be byte-identical.

Primary evidence in capture.sha256 and later schema2-evidence.sha256:

- attempt.txt and capture-complete.txt: reviewed package/historical receipt; retained
  on failure, no resumption/completion synthesis after interruption.
- prior-evidence.txt: hashes of B7.5 final data/seal and B7.7 initial/final reference;
  Linux unsealed/UNRESOLVED status remains explicit. Old artifacts are never changed.
- source2-before.hex.tsv / source2-after.hex.tsv: full nine-table canonical exports.
- source2-before.ddl.txt / source2-after.ddl.txt: raw nine-table SHOW CREATE exports.
- schema2.sql, dump-exit.txt (must be0), dump-validation.txt: plain full native dump
  and offline confirmation of every row/column value against the export, including
  all history, outbox, schema migration and attempt journal data.
- health-before.tsv / health-after.tsv: complete zero-valued reviewed health rules.
- grants-before.tsv / grants-after.tsv: exact native identity/account/object/session
  metadata, equal before/after; no authentication data.
- preservation.txt: original-row comparison/counts and unresolved Linux exceptions.
- client-version.txt / dump-version.txt; environment.json and untouched timestamped
  environment JSON/checksum named by environment-origin.txt. All join the manifests.
- capture.sha256: capture integrity before private review.
- private-review.txt and schema2-evidence.sha256: added ONLY after explicit review;
  final manifest includes capture.sha256, primary evidence and review attestation.

Any drift/health failure/missing table or row/changed history/extra authority stops
before final sealing. Unsupported dumper syntax also stops; never bypass parsing
or broaden privileges. The offline parser checks table/column coverage and exact
values, not the full semantic equivalence of every CREATE constraint or SQL session
expression. Full private SQL review is mandatory, and B7.9 restored structure/data
verification remains a separate future gate; this backup is not a restore pass.

## Exact read-only SQL/tool procedure

Pinned source files: scripts/phase12/b75/preflight.sql (SELECT metadata only),
b7/public-grants.sql (Windows SHOW GRANTS FOR PUBLIC), b7/schema2-export.sql,
b7/ddl2-export.sql and b7/schema2-health.sql. No new SQL/migration is introduced.
Authentication probe is SELECT DATABASE(),CURRENT_USER(); built TaskCompassMigrate
runs `--verify --to-version 2` and offline
`--print-migration 1` / `--print-migration 2` checks, never --apply. B7.5 empty-organization postconditions
are deliberately NOT used after populated GUI testing.

Reviewed text sources/manifests have explicit LF checkout attributes; no existing
private artifact is rewritten or normalized. ReviewChecksums.sha256 pins the B7.8
operator sources, guide and reused dependencies; its digest joins the capture
receipt so a changed package cannot silently seal an earlier capture.

The capture's native dump argv, with the diagnostic password supplied through the
in-memory environment (not a shell-history secret), is:

```text
mariadb-dump --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306
 --user=taskcompass_dev --default-character-set=utf8mb4 --single-transaction
 --skip-lock-tables --no-tablespaces --skip-triggers --skip-add-drop-table
 --skip-add-locks --skip-disable-keys --no-create-db --hex-blob
 --result-file=<new private snapshot>/schema2.sql taskcompass_dev
```

Windows converts only that result-file path to native form. Require exit0 and a
nonempty file. With no routines/events/triggers/views permitted by preflight, no
stored code is omitted; no LOCK/PROCESS/DDL account rights are added. Same snapshot
flags as the existing reviewed B7.8 proposal, now scripted with complete evidence.

## Private SQL review, stops and recovery

Read **every** statement privately. Expect exactly nine plain InnoDB table
CREATEs with the matching columns/PKs/FKs/CHECKs/indexes, their INSERT data, normal
session-only SET/save/restore settings (possibly inside version comments), comments
and the recognized MariaDB sandbox directive. SQL keywords inside task/JSON string
values are data, not executable statements. Reject database/account/grant changes,
DROP/TRUNCATE/REPLACE, LOCK/ALTER-disable-keys, GLOBAL/PERSIST changes, stored code,
DEFINER, redirected writes or foreign-schema targets. Do not execute the dump.

On ANY unexpected result: stop, retain actual database state and every directory,
attempt marker and file; report stage/platform/last PASS/error. Do not retry the
dump to that path, seal a failed attempt, delete a directory or repair a checksum.
A new capture after a failure requires explicit review/authorization; scripts never
resume one. A missing --dump-reviewed is a precondition rejection before any review
receipt/seal is written; privately inspect before invoking the review stage correctly.
An interrupted review can leave partial attestation/seal artifacts: preserve them
and request a specific decision, rather than replaying or deleting markers.

There are no database writes to roll back in B7.8. Existing dumps are never restored
as automatic recovery. Preserve all Phase1.1/B7.2/backups, completed rehearsal and
integration databases, B7.5/B7.7 evidence and QA records. Do not recreate missed
Linux comparisons or undo the intentional user organization to pass a check.

## Approval Requested — first live stage: Linux02 ONLY

Authorize owner execution of read-only **02-preflight.sh --approved --platform linux**
on feledria/MariaDB10.5.29, taskcompass_dev@127.0.0.1:3306, dataset
21be9da7-b767-4578-b972-cc9a35122ddb. Scope: existing-evidence reads; three existing
account identity/schema probes; existing sudo/socket root metadata/PUBLIC policy
where applicable; reviewed health SELECTs; one nine-table data stream hashed for
comparison; local environment/version notes. Writers closed. No new dump or backup
folder, grant, data mutation, account/plugin/password change, restore or cleanup.
On failure preserve state/evidence and report; no automatic retry/repair. Stages03/04
and all Windows live actions remain unauthorized until separately approved.
