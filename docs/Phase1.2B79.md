<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.9 — isolated populated schema-2 restore validation

Preparation only. No live stage is authorized by this guide. Linux first;
review each result before approving the next stage. Windows needs its own
platform/stage approvals. B7.10 and Phase1.3 are outside scope.

This implements the existing [B7.9 design](Phase1.2B7.md#b79--isolated-schema-2-restore-preserve-populated-rehearsal)
with five separate stages: preflight, provisioning, import, verification, and
privilege reduction. Compared with the historical shell recipe, evidence is
written into a **new sibling directory**, never into a sealed B7.8 directory.
No grouping combines provisioning with import or verification with revocation.
There are no typed approval phrases. `--approved` is an opt-in for the separately
authorized platform/stage; it is not authorization for later operations.

## Fixed identities and inputs

| Platform | Server / TCP port | Dataset | Sealed snapshot |
| --- | --- | --- | --- |
| Linux | feledria / 10.5.29-MariaDB / 3306 | 21be9da7-b767-4578-b972-cc9a35122ddb | /home/justin/temp/TaskCompassBackups/Phase1.2-schema2-linux-20261120-204710-103433 |
| Windows | feldria-w10 / 13.0.2-MariaDB / 3306 | 457bb52b-fea4-4347-9c47-675c6b035c90 | C:/Users/justin/TaskCompassBackups/Phase1.2-schema2-windows-20261121-222115-177176 |

Only target: database `taskcompass_phase12_restore`, account
`taskcompass_phase12_restore@127.0.0.1`. No CLI target/snapshot override exists.
The application database allowlists remain unchanged; never run the GUI,
TaskCompassMigrate, initializers, or integration/reset tests against this target.
Do not access or modify development, integration, or rehearsal data. Only the
new restore target and narrowly scoped server/account metadata are queried.
Preserve the failed Windows capture ending `20261121-192804-176636` untouched.

The original B7.8 reviewed package checksums are pinned independently:

- Linux: `46dbffef69d1d85eabbc9982a190dbff81466190a075810ab259200d3a4ed4b5`
  (recorded original B7.8 package in the milestone).
- Windows: `f0203488fd1b019f8a9475d0c4ac41d940700550c658d3723831cf7e225291ad`
  (review manifest at owner-reported capture source commit
  `eb7bac1b426e64f6f1bcae044a15e770773ae431`, independently read from Git).

Every stage reads and checks both original B7.8 manifests with exact filename
coverage, the timestamped environment sidecar, original capture/package/platform
receipt, private review attestation, native client version and captured server
identity. It verifies before/after source bytes, dump exit0, Dataset UUID, both
migration checksums/timestamps, completed MigrationAttempt, and all nine tables'
values in the dump. It derives the journal's schema2 DDL fingerprint using the
same table order/table-name-case normalization as MigrationRunner. Historical
schema1 fingerprint is checked against the completed attempt's baseline; schema1
DDL cannot be recomputed from a schema2 snapshot. No migrations are replayed.
All 85 column names are checked at the restored server, and exact raw DDL equality
with the sealed source covers their types, indexes, foreign keys and constraints.
No index-alias exception is used for restore equality: unexpected bytes stop.

Actual dump SHA256 and final seal SHA256 are calculated from the approved private
files at stage01, printed for review, and bound in `binding.json`. They cannot be
known from preparation without private-file access. Later stages revalidate these
hashes and bind to the same B7.9 source package. No source-current-development
comparison is substituted for snapshot comparison. Later edits in development
are irrelevant; this process does not query its domain data.

## Offline preparation checks — no live database

Run from the repository root on each platform:

```bash
scripts/phase12/b77/01-build.sh
ctest --test-dir build -R '^Phase12Schema2RestoreTests$' --output-on-failure
```

On Windows use `--test-dir build-ucrt64` in the second command. The existing build
helper collects the environment report and runs the offline suite (integration
entries stay disabled/skipped without their isolated opt-in). Do not configure
live test environment variables. Native Python3, Bash, the existing native
MariaDB client and Python standard library are used; no dependency installation.
Mock tests use temporary synthetic snapshots and never default to a real client.
Both simulated platforms exercise UTF-8, Windows CRLF, spaces in paths, complete
success and refusal/partial-operation cases. Native Windows is still required
for the new CLI/PowerShell/path combination; mocks do not prove compatibility.
`Phase12RestoreAclTests` is registered only on Windows and exercises the real
privacy wrapper on a uniquely created disposable temp fixture, including rejection
of extra file access. Its cleanup affects only that test fixture, never backups
or databases. Direct native offline command:

```bash
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$(cygpath -w tests/Phase12RestoreAclTests.ps1)"
```

PowerShell file invocations use per-process `-ExecutionPolicy Bypass`; no saved
policy changes. The directory/file privacy wrapper uses the reviewed B7.8 policy:
owner-only protected directory ACL, owner-only files, no reparse points. Linux
requires current UID, directory0700 and file0600. New evidence directory creation
sets permissions on that empty directory only. Snapshot ACLs are checked read-only.

## Operator stages and separate approvals

Close other clients to the new restore target before each stage. Sessions belonging
to this operator's identity-checked reader are recognized by connection ID; any
other target account/database session stops the stage. No task writers need to
be launched. The source database is not used or altered.

### Linux

After **Linux stage01 approval only**:

```bash
scripts/phase12/b79/01-preflight.sh --approved --platform linux
```

Expected: source manifests/private permissions/provenance/dump/UUID/journal checks
pass, native client matches, server identity matches; restore database and account
are absent on **every host**, no conflicting sessions/objects. A new directory
alongside the snapshot is printed as `Private B7.9 evidence directory:`:
`Phase1.2-restore-linux-YYYYmmdd-HHMMSS-RANDOM`. It contains `binding.json`, native
client version, preflight metadata, attempt/success receipts, environment report
and `preflight.sha256`. This stage performs read-only SQL; local evidence creation
is its only persistent effect. Do not rerun if interrupted/failed without review.

Set the actual printed path (not a placeholder), then report stage01 output and
schema2.sql SHA256 for review. Subsequent commands each require their own approval:

```bash
# Run separately, only after approval for that one stage.
scripts/phase12/b79/02-provision.sh --approved --platform linux --run-dir "$restoreDirectory"
scripts/phase12/b79/03-restore.sh --approved --platform linux --run-dir "$restoreDirectory"
scripts/phase12/b79/04-verify.sh --approved --platform linux --run-dir "$restoreDirectory"
scripts/phase12/b79/05-readonly.sh --approved --platform linux --run-dir "$restoreDirectory"
```

Do not paste all four as an authorized sequence. Stage02 prompts for a new 12+
character password; other stages request that account's password. Linux sudo
prompts normally for administrator access. No password is written to disk, argv,
SQL files, notes or history; it exists in memory and the child client's process
environment only. Avoid putting secrets in `--client`/`--run-dir` values.

### Windows UCRT64

After Linux results are reviewed and **Windows stage01 separately approved**:

```bash
scripts/phase12/b79/01-preflight.sh --approved --platform windows
```

Use native Windows Python (not MSYS Python). Default native client is
`C:/Program Files/MariaDB 13.0/bin/mariadb.exe`. The wrapper converts its own Python
script path; native Python handles all snapshot and SQL paths, and SQL is sent by
stdin rather than shell-redirection path assumptions. `MYSQL_PWD` is excluded
from MSYS path conversion. Output paths are native `C:/...`; copy the printed
path into `restoreDirectory` in your UCRT64 shell. Optional `--client` and
`--build-dir` overrides also use native `C:/...` paths, not `/c/...`.
Windows root and restore-account passwords are separate prompts.

After each separate approval, run only the applicable command:

```bash
scripts/phase12/b79/02-provision.sh --approved --platform windows --run-dir "$restoreDirectory"
scripts/phase12/b79/03-restore.sh --approved --platform windows --run-dir "$restoreDirectory"
scripts/phase12/b79/04-verify.sh --approved --platform windows --run-dir "$restoreDirectory"
scripts/phase12/b79/05-readonly.sh --approved --platform windows --run-dir "$restoreDirectory"
```

Windows PUBLIC rights are inspected without executing grants. Only the known
unrelated `test` / escaped `test\_%` scopes or USAGE are accepted; any unknown
scope, inherited role or unexpected statement stops for review. Individual target
account rights are exact, including host variants, global/table/column/routine/
proxy privileges and role mappings. Do not broaden the policy to bypass a failure.

## Exact modifications and pass criteria

Stage02 uses only [restore-account.template.sql](../scripts/phase12/b7/restore-account.template.sql),
rendering the password token in memory under NO_BACKSLASH_ESCAPES:

```sql
CREATE DATABASE taskcompass_phase12_restore CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_phase12_restore'@'127.0.0.1' IDENTIFIED BY <in-memory literal>;
GRANT SELECT, INSERT, CREATE ON `taskcompass\_phase12\_restore`.* TO 'taskcompass_phase12_restore'@'127.0.0.1';
```

The administrator session is checked without a selected database and reused for
the modification and postchecks. No IF NOT EXISTS, CREATE OR REPLACE, DROP, ALTER,
UPDATE, global rights or GRANT OPTION. Metadata confirms exact three privileges
and an entirely empty target. A newly opened target-account connection confirms
server/database/current-user identity. MariaDB DDL/accounts/grants are not assumed
transactionally reversible. Partial provisioning is preserved without repair.

Stage03 requires the successful stage02 manifest and absent import marker. It
rechecks snapshot hashes, account and session identity, empty target and exact
privileges. An exclusive attempt marker precedes one import in the checked
restore-account session, not root. A completion sentinel and successful native client exit are required; the import
exit code is recorded as restore-exit.txt. No migrations/dataset initialization/reconnect,
`--force`, implicit retry or LOAD LOCAL INFILE. Dump session settings are confined
to that connection. On success require exactly nine InnoDB tables/85 columns,
no views/routines/triggers/events/cross-schema foreign keys. Actual row/constraint
validation is mandatory in stage04: reenabling FK checks does not validate old rows.

Stage04 uses fresh administrator/reader identity checks and writes canonical
`verify-data.hex.tsv`, raw `verify-ddl.txt`, `verify-health.tsv` and comparison
report. Both exported byte streams must exactly match sealed B7.8 source bytes.
Dataset identity, complete migration journals/attempt and fingerprint are checked
independently again. All source rows/counts/ownership/audit/outbox payloads survive;
all health rules must be present with zero violations. No new writes to the target.

Stage05 first repeats full data/DDL/health verification, then executes only
[restore-account-readonly.sql](../scripts/phase12/b7/restore-account-readonly.sql):

```sql
REVOKE INSERT, CREATE ON `taskcompass\_phase12\_restore`.* FROM 'taskcompass_phase12_restore'@'127.0.0.1';
```

Verify exact SELECT-only schema rights, close the old target reader, reconnect
with fresh identity, repeat full equality and health. No deliberate rejected
write probe is needed; exact grant metadata proves revocation without a write.
Preserve the populated restore database/account and every evidence artifact.

## Evidence, failures and retest

Each stage writes exclusive `STAGE-attempt.txt`, before/after metadata where
applicable, an environment report, `STAGE-success.txt`, and `STAGE.sha256`. Seals
cover every file then present, including prior manifests. `binding.json` contains
platform/fixed target/snapshot path, dump/final-seal hashes, original B7.8 package
and current B7.9 package hashes. Evidence from another package/run is rejected.
The shared environment reporter automatically records source, build/Qt, server
metadata provenance and timestamped changes without new database connections.

Any existing attempt/success/seal for the current or later stage refuses replay.
A missing/tampered prerequisite seal refuses progression. Stop on wrong server,
existing target/host variant, changed dump, unexpected authority, partial import,
DDL/data/health mismatch, unexpected objects or interruption. Client stderr is
reduced to numeric error/SQLSTATE diagnostics so echoed SQL, passwords and data
cannot enter evidence; failure messages name the operation. Process output is
UTF-8; machine exports retain native output bytes and require exact source equality.
No sealed artifact is normalized to hide a mismatch.

On failure preserve target state and all artifacts. Report operation/error code,
printed directory and file inventory without sharing private exports. Do not
rerun, delete markers, resume, repair, reset, restore over, drop, rename, broaden
privileges or clean up. Request a specific reviewed recovery decision. Connection
loss, timeout or interruption may leave partial committed DDL/data; closing the
client does not undo it. No automatic recovery is implemented.

Known Linux B7.7 exceptions remain explicit for B7.10: protected Task
`24dd11c3-59df-4e23-ac21-422bcf2a1e3a` changed updatedAt/localRevision/objectiveId,
and the restart comparison is missing. A perfect restore proves this captured
snapshot is recoverable; it does not repair or waive those historical exceptions.
