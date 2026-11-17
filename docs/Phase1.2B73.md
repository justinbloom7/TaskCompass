<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.3 — approved operator-only populated restore rehearsal

Owner approved B7.3 on both platforms on 2026-11-17. Approval and historical results
are in [Phase1.2.md](milestones/Phase1.2.md); underlying reviewed procedure is
[Phase1.2B7.md](Phase1.2B7.md#b73--populated-schema-1-recoveryrehearsal-clone).
The agent has **not** run any live SQL, read private backups or performed a restore.

Update the scripts in each checkout through the owner's normal source workflow.
Run one stage at a time from repository root, Linux Bash or Windows MSYS2 UCRT64.
Keep TaskCompass/SQL writers closed; leave MariaDB running. Use native tools from
the known local server installation. No dependencies are installed or grants changed.

| Platform | Exact default snapshot directory | Bound after-gui.sql SHA-256 |
| --- | --- | --- |
| Linux | $HOME/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703 | 83ae094ed33865bba0c22e39174f2977a49cc014de3f669115300105eec2bd26 |
| Windows | $HOME/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917 | 6588eb9ebc30b63b36b33f2abbc4a0de9da31b252c41a3fd585b8b060073fe50 |

The scripts fix the database/account to **taskcompass_phase12_test**, TCP127.0.0.1,
port3306, and each known platform's hostname/version/dataset. Never use the older
restored taskcompass_integration or taskcompass_dev as the destination. No source
DB writes, migration002, account provisioning, privilege changes, GUI writes or reset.

## 1 — snapshot integrity and read-only empty-target/grant preflight

```bash
scripts/phase12/b73/01-preflight.sh --approved
```

The script validates private directory permissions/ACL and all sealed B7.2 artifacts
through the existing B7.2 checker in **checks-only** mode, then checks the exact
approved platform dump digest. It does not open the pager or claim a new SQL review.
The previous owner review remains bound to the unchanged digest. No files change.

Root metadata preflight uses Linux local socket/sudo (OS password when needed) or
Windows native TCP/root password prompt. Administrator passwords are not captured
or cached by the scripts. The new [preflight.sql](../scripts/phase12/b73/preflight.sql)
contains SELECTs only over server/schema/grant metadata, not unrelated domain data.
Windows also reads PUBLIC grants. Exact automatic guards require:

- feledria/10.5.29-MariaDB or feldria-w10/13.0.2-MariaDB, port3306, selected
  taskcompass_phase12_test and root@localhost; expected table-case mode0/1 and
  FK/CHECK enforcement1.
- Existing utf8mb4/utf8mb4_unicode_ci target, **zero** tables/views/routines/triggers/
  events and no other selected-target sessions. Never reset a nonempty target.
- Exactly test account@127.0.0.1, six schema-scoped SELECT/INSERT/UPDATE/CREATE/DROP/
  ALTER rights on the escaped exact schema; global USAGE only, no extra table/
  column/role/grant-option authority. Scope-widening wildcard grants fail.
- PUBLIC cannot grant writes/DDL covering source/target; inherited PUBLIC roles
  require review. Unrelated test/test\_% grants are accepted. PUBLIC not queried
  on MariaDB10.5, where that feature is unavailable.

Expected: reviewed checksum entries OK and `PASS` for the correct empty fixed target
and exact grants. **Stop on any error or mismatch**, even if caused by root login;
no import, reset, provisioning or repair is attempted. Review the report before step2.

## 2 — exact approved snapshot import, one shot

```bash
scripts/phase12/b73/02-import.sh --approved
```

The script prints the platform's exact approved hash. Confirm:

```text
RESTORE taskcompass_phase12_test
```

Then:

```text
DUMP REVIEWED; WRITERS CLOSED
```

Use this platform's **taskcompass_phase12_test** password when prompted. It gets
up to three authentication-only attempts and is kept in memory for this run; no
credential files or passwords in arguments. Native login/identity probe is read-only.
Network/identity/unknown errors and subsequent operation failures are never retried.
Root metadata preflight is repeated immediately before import: sudo may remain
cached on Linux; Windows will request its root password again. This independent
admin prompt is intentional to recheck empty target/current grants, not import as root.

Before any import SQL the script rechecks the snapshot and writes a new private
`b73-import-attempt.txt` marker (approved hash only) with noclobber. It then imports
only after-gui.sql into the explicit test target using the test account and same-
platform native client. No --force, DROP/reset command, new dataset initialization
or migration apply. Import contains the privately reviewed five CREATE TABLE/INSERT
snapshot and its normal session prologue/epilogue, as approved by its digest.

Expected: `PASS: one-shot import exit 0`, with a new b73-import-success.txt marker.
**Go to step3, never back to step1/import or the disposable integration suite.**

On error/uncertain result the attempt marker and partial database remain. Do not
remove/copy around markers, edit snapshots, reimport, reset or repair. Even if the
target happens still to be empty, new recovery approval is required. An existing
attempt/success/comparison marker blocks another import before connecting. DDL is
not a rollback bundle. Preserve all diagnostics/artifacts and report the failure.

## 3 — restored schema, UUID and full data/DDL preservation

```bash
scripts/phase12/b73/03-compare.sh --approved
```

Enter **taskcompass_phase12_test** password again: each script is a separate run,
so no persistent credential cache exists. This stage requires both successful
import markers with the exact approved hash. It uses guarded Qt verification:
`--verify --phase12-test --to-version 1`, explicit approved test environment, strict
session, shape/FK endpoint/index checks and unchanged full DDL fingerprint.

Expected schema1 dataset UUID:

- Linux: 21be9da7-b767-4578-b972-cc9a35122ddb.
- Windows: 457bb52b-fea4-4347-9c47-675c6b035c90.

Then it creates only new private rehearsal1.hex.tsv/rehearsal1.ddl.txt exports and
b73-comparison.sha256. Every one of the original 38 columns/all row counts,
immutable audit payload bytes, tombstones, completion/timestamps/revisions and
SchemaMigration row1 must match the sealed canonical source export; raw DDL must
also match the same-platform source capture. No native error or mismatch is ignored.

Expected: schema1/same UUID, new comparison checksums OK and:

```text
PASS: B7.3 schema1/UUID/all legacy columns and history/counts/DDL match exact snapshot.
```

No existing evidence is overwritten. Partial comparison files or any failure stop
for review; do not rerun with renamed output files to bypass guards. Successful
comparison is not a permission to run migration002 or change the test role yet.

## Report and preserve

Share platform, preflight/import/compare PASS or errors, verified UUID and comparison
checksum results. No passwords, dumps or data exports in chat or Git. Keep source
capture and populated clone intact. Clone UUID intentionally matches its source;
this is a passive rehearsal copy, not a syncing client.

**Never run reset-capable integration tests on the now-populated rehearsal target.**
Original restored taskcompass_integration and all Phase1.1 evidence remain untouched.
B7.4 minimum-privilege migration/runtime rehearsal requires separate approval.
No cleanup follows success. Agent may prepare scripts/tests/docs but must not run
these live procedures on the owner's behalf.

All scripts accept --help/--print-plan without connection and require --approved
for actual stages. Optional trusted --client/--build-dir and --backup-dir support
installed paths/copies of the identical pinned artifact; a relocated snapshot still
requires authorization for the location, and its digest cannot be overridden.
No database/user/host/hash override or test-only bypass is provided. No extra target
is allowlisted in the application. Runtime tooling uses existing Bash/native tools;
the offline fake-executable test uses existing Python3, not a new runtime dependency.

B7.2 --checks-only intentionally performs no pager/SQL safety assertion: explicit
reviewed-dump confirmation at import and the previous owner review of the pinned
bytes remain required. Bounded credential entry is shared with B7.2 through a
small fixed-pair read-only helper; it permits only dev's three existing accounts
and the exact Phase1.2 test pair. No generic database credential system was added.
