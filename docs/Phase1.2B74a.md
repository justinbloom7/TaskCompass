<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.4a — Populated clone migration rehearsal

Owner approval on 2026-11-17 covers both platforms, operator execution only.
The [milestone](milestones/Phase1.2.md) records approval and results; the
[review package](Phase1.2B7.md) contains the exact SQL and recovery policy.
No live B7.4a operation has been executed by the agent.

Use the updated repository and built migration executable. Run these **one at a
 time**, stopping on any error, in repository-root Bash on Linux or MSYS2 UCRT64
on Windows. The commands are identical; platform, client, executable and approved
snapshot paths are selected automatically:

```bash
scripts/phase12/b74a/01-preflight.sh --approved
scripts/phase12/b74a/02-grants.sh --approved
scripts/phase12/b74a/03-migrate.sh --approved
scripts/phase12/b74a/04-preservation.sh --approved
```

Do not paste all four commands as an unattended batch. Close all TaskCompass
writers first. Do not run disposable integration/reset tests against these clones.

## Targets and credentials

Only `taskcompass_phase12_test`, account `taskcompass_phase12_test@127.0.0.1`,
TCP port3306 is changed. Linux expects feledria/MariaDB10.5.29 and dataset
`21be9da7-b767-4578-b972-cc9a35122ddb`; Windows expects
feldria-w10/MariaDB13.0.2 and dataset `457bb52b-fea4-4347-9c47-675c6b035c90`.

The sealed capture directory defaults are:

- Linux: `/home/justin/temp/TaskCompassBackups/Phase1.2-schema1-linux-20261117-013703`.
- Windows: `$HOME/TaskCompassBackups/Phase1.2-schema1-windows-20261117-064917`.

Enter the **test account password for that platform**, not the diagnostic,
runtime or migrator password. Authentication-only probes permit three attempts;
checked test credentials remain in memory only for that script run. Subsequent
operation failures never trigger a retry. Linux root uses local `sudo mariadb`
socket authentication; Windows root uses the native client's password prompt.
Administrator passwords are never captured or cached by the scripts. Each stage
uses one administrator session; stage02 does not ask for another root password
following the privilege change. TLS settings are unchanged.

## Stages and expected results

1. **Preflight:** verify original private snapshot checksums, B7.3 receipts/exports,
   source and embedded migration SQL digests, native client version, schema1
   fingerprint, UUID, all legacy values and DDL, server identity and exact original
   six schema grants. Reject unexpected objects, stored code, other selected-schema
   sessions, roles, grants or effective PUBLIC writes. This stage changes no files
   or grants. Expected PASS for the populated schema1 baseline.
2. **Grants:** repeat fresh baseline verification before the exact reviewed
   [grant transition](../scripts/phase12/b7/rehearsal-migration-grants.sql), then
   verify narrow grants and unchanged schema1 data again. Revoke the six broad
   schema grants; retain schema SELECT and grant only the reviewed ten individual
   table privileges. No password/account change, global grant, DROP privilege or
   migration is introduced. Expected PASS: narrow transition; no migration yet.
3. **Migration:** require successful grant receipts, freshly revalidate schema1
   and narrow privileges, then execute only the reviewed migration runner:
   `TaskCompassMigrate --apply --phase12-test --to-version 2`. Independently verify
   schema2 and the unchanged UUID. Expected PASS: one-shot migration002. This is
   not yet the complete preservation verdict.
4. **Preservation:** verify schema2 and narrow privileges, export every original
   column/history payload and compare with the sealed snapshot. Check both
   migration rows and the completed nine-step journal, empty organization tables,
   NULL legacy task owners, unchanged typed targets/payload versions, and zero
   violations for all59 CHECK/FK health queries. Seal new exports and receipts.
   Expected final PASS: schema2 clone with original data/history preserved.

Stage02 now prints these notices without asking for typed confirmation:

```text
NARROW GRANTS taskcompass_phase12_test
WRITERS CLOSED; BASELINE UNCHANGED
```

Stage03 notices:

```text
MIGRATE REHEARSAL taskcompass_phase12_test TO 2
WRITERS CLOSED; BASELINE UNCHANGED
```

## Failure and evidence policy

Grant and migration attempt markers are written **before** mutation; success
markers are written only after verification. They bind the snapshot, migration002
and exact grant-file digests. Existing markers/exports are never overwritten.
New evidence is stored beside the retained capture: `b74a-grants-{attempt,success}.txt`,
`b74a-migration-{attempt,success}.txt`, `rehearsal2-legacy.hex.tsv`,
`rehearsal2.ddl.txt`, `b74a-upgrade-checks.tsv`, `b74a-health.tsv` and
`b74a-preservation.sha256`.

Stop on any unexpected result, disconnect, interruption or partial attempt.
Do not delete/rename markers, retry mutation stages, resume SQL, broaden grants,
reset, repair or restore in place. MariaDB DDL/grants are not transactionally
reversible. Preserve the failed clone and all artifacts; report the exact error
for explicit recovery approval. Closing a hung client does not prove its SQL did
not execute. An isolated recovery target/restore requires new approval.

Neither development database/account, Phase1.1 backups or restored integration
databases are modified. B7.4b runtime-role rehearsal, B7.5 development upgrade,
GUI acceptance and Phase1.3 remain unauthorized. Preserve successful schema2 clones.

## Offline validation

```bash
cmake --build build --parallel 2
ctest --test-dir build -R '^Phase12MigrationRehearsalScripts$' --output-on-failure
```

On Windows substitute `build-ucrt64`. The tests use fake clients/snapshots only.
Passing mocks is not a native Windows session or live MariaDB rehearsal result.
