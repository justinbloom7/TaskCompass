<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.4b — Runtime-role rehearsal

Approved by the owner on 2026-11-17, for operator execution on both populated
schema2 rehearsal clones only. See the [milestone](milestones/Phase1.2.md) and
[exact reviewed B7.4b procedure](Phase1.2B7.md#b74b-runtime-role).
No live B7.4b operation has been executed by the agent.

Update the repository/build first. Close all TaskCompass writers. From the
repository root, run **one command at a time**, stopping on any error:

```bash
scripts/phase12/b74b/01-preflight.sh --approved
scripts/phase12/b74b/02-runtime-grants.sh --approved
scripts/phase12/b74b/03-probe.sh --approved
```

These commands are identical in Linux Bash and Windows MSYS2 UCRT64. Do not rerun
B7.4a mutation scripts or disposable integration/reset tests.

## Accounts, targets and prerequisites

Only `taskcompass_phase12_test` and its existing same-named account at
`127.0.0.1:3306` are used. Enter this platform's **test account password** when
prompted. It permits three authentication-only attempts and retains checked
credentials in memory only for that script run. Administrator authentication stays
in the native client: sudo/socket on Linux, Windows MariaDB root password on
Windows. TLS settings remain unchanged. No administrator password is stored.

The scripts retain B7.4a's exact platform/server/version/UUID and snapshot pins:
Linux feledria/10.5.29 and dataset `21be9da7-b767-4578-b972-cc9a35122ddb`;
Windows feldria-w10/13.0.2 and dataset `457bb52b-fea4-4347-9c47-675c6b035c90`.
They use the same independently captured private directories as
[B7.4a](Phase1.2B74a.md). B7.2/B7.3 and all eight sealed B7.4a receipts/exports
must still validate. Fresh Qt schema2/UUID verification, exact legacy/DDL/journal/
health comparisons and administrator identity/grant/session checks are mandatory.
No dev database or account is accessed or changed.

## Expected results

1. **Preflight:** fresh schema2, evidence and exact narrow migration privileges
   match. No mutation or evidence overwrite.
2. **Runtime grants:** freshly verify again, capture all nine tables privately,
   apply only [rehearsal-runtime-grants.sql](../scripts/phase12/b7/rehearsal-runtime-grants.sql),
   then require exactly19 table privileges and no schema/global/column privileges,
   roles or grant option. Reverify Qt read access, legacy data/DDL/journal/health
   and complete nine-table equality. No probe yet.
3. **Probe:** freshly verify runtime grants and unchanged full data; run only
   [runtime-probe.sql](../scripts/phase12/b7/runtime-probe.sql) in one new native
   client session, with no automatic reconnect or `--force`. Require four UPDATE
   counts1 and six post-ROLLBACK probe-row counts0. Require complete before/after
   equality of all nine tables and repeat Qt/schema/legacy/journal/health checks.
   Final PASS means the runtime SQL permission rehearsal passed, not GUI acceptance.

Stage02 prints these notices without requiring input:

```text
RUNTIME GRANTS taskcompass_phase12_test
WRITERS CLOSED; BASELINE UNCHANGED
```

Stage03 prints these notices without requiring input:

```text
ROLLBACK PROBE taskcompass_phase12_test
WRITERS CLOSED; BASELINE UNCHANGED
```

## Evidence and failure policy

New private artifacts beside the original capture are
`b74b-before-grants.hex.tsv`, `b74b-grants-{attempt,success}.txt`,
`b74b-grants.sha256`, `rehearsal-before-probe.hex.tsv`,
`rehearsal-after-probe.hex.tsv`, `b74b-probe-result.tsv`,
`b74b-probe-{attempt,success}.txt` and `b74b-probe.sha256`.
Receipts bind the original snapshot, migration and both approved runtime SQL
hashes. Attempt markers precede mutation; existing attempts prevent replay.
Original evidence and new files are never overwritten.

Grant changes are nontransactional and can partially succeed. On any error,
interruption or uncertain outcome, stop and preserve state/artifacts. Do not retry,
reset, repair, resume, broaden privileges, rename markers or restore in place.
The probe contains explicit ROLLBACK and no COMMIT/DDL; disconnect rolls back its
uncommitted InnoDB transaction, but an uncertain outcome still requires review.
Any recovery requires separate explicit approval based on observed state.

Preserve both successful clones and all Phase1.1 evidence. Development upgrades,
B7.5/B7.6, GUI writes and Phase1.3 are not authorized by this rehearsal approval.
Report both platform results before requesting the exact development approval.

## Offline test

```bash
ctest --test-dir build -R '^Phase12RuntimeRehearsalScripts$' --output-on-failure
```

Use `build-ucrt64` on Windows. Mocks do not validate native Windows execution or a
live MariaDB connection. No new dependencies or production behavior are introduced.
