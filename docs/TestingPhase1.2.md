<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 source review and isolated validation

**Current pointer (2026-11-20): both platform GUI functional acceptances are
recorded. Linux preservation/restart exceptions remain unresolved. B7.8 backup
preparation and its Linux-first approval scope are in [the operator guide](Phase1.2B78.md);
no live B7.8 operation has been performed by the agent. Earlier pending statements
below are historical and superseded by the milestone's current status.**

The [milestone record](milestones/Phase1.2.md) is authoritative for approvals and
actual results. **Linux Stage 1A and corrected Stage 1B passed (owner-reported);
Windows Stage 1A and Stage 1B also passed. B7.1 read-only development preflight,
including corrected Qt schema-1 verification, passed on both platforms on
2026-11-17 (owner-reported). B7.2 fresh schema-1 backup/exports are approved for
owner execution only; [numbered operator scripts](Phase1.2B72.md) are prepared.
B7.2 capture/sealing and owner-confirmed private SQL review passed on both platforms
on 2026-11-17; retained paths/hashes and historical failures are in the milestone.
B7.3 populated exact-snapshot isolated restore is approved for owner execution
using [three numbered scripts](Phase1.2B73.md); Linux offline suite18/18 passed.
B7.3 populated restore/schema1/full preservation passed on both platforms
(owner-reported); both populated clones must be retained. B7.4a narrow-migrator
grant/clone migration is approved for owner execution using
[four numbered scripts](Phase1.2B74a.md); all four stages passed on Linux and Windows (owner-reported, 2026-11-17). B7.4b runtime-role rehearsal is approved for owner execution using
[three numbered scripts](Phase1.2B74b.md); all three stages passed on Linux and Windows
(owner-reported, 2026-11-17). B7.5 development upgrade scripts and
[operator guide](Phase1.2B75.md) are prepared. Linux stages01/02 and reviewed schema2 preservation completion passed
(owner-reported, 2026-11-17), after the recorded implicit-index comparison correction.
Linux runtime transition04 and final verification05 also passed (owner-reported,
2026-11-17). Linux development upgrade/final privileges/preservation/health are
verified; Windows B7.5 stages01–05 also passed (owner-reported, 2026-11-17).
Both development upgrades, final least-privilege grants, original preservation and
health are verified; earlier authentication failures remain historical.
No live procedure was performed by the agent. B7.7 GUI acceptance preparation
is now available in the [guide](Phase1.2B77.md), [Linux checklist](Phase1.2B77Linux.md)
and [Windows checklist](Phase1.2B77Windows.md). Linux read-only02 preflight passed (owner-reported,2026-11-18). The first03 launch
was rejected for a placeholder directory; GUI/manual results remain pending.
Windows GUI QA remains separately gated. Backup/restore and final acceptance remain separately gated.** Do not execute
steps 3 or 4 without separate approval for that step, platform and target. The earlier isolated steps below do not upgrade taskcompass_dev; completed
B7.5 development results are recorded separately above. Preserve taskcompass_integration, all accepted
Phase 1.1 records and private backups/exports. Do not use its old reset scripts here.

Use Bash on CentOS Stream 9 or the MSYS2 **UCRT64** terminal on Windows. Prerequisites
remain in [Installation](Installation.md): C++20 compiler, Qt 6 Widgets/SQL/Test,
loadable QMYSQL and compatible MariaDB client libraries, CMake, Ninja, Bash,
coreutils (including sort/sha256sum), and MariaDB server/client for the later live
steps. No script installs packages or changes service/TLS settings. CTest uses only
fake clients/in-memory repositories unless explicitly opted into a live suite.

## 1 — Build and run offline tests (safe now)

**2026-11-18 portability correction:** owner's Linux B7.7 build compiled, but two
offline tests failed because they accidentally invoked optional ripgrep. The tests
now count fake mutations with Bash built-ins; no ripgrep installation is required.
Both corrected tests passed agent rerun with an executable rg sentinel that fails
if invoked. Owner Linux focused retest subsequently passed2/2 (34.71 seconds,
2026-11-18); the original failure remains recorded. Focused retest command:

```bash
ctest --test-dir build -R '^Phase12(ReviewedCompletion|UpgradeInspection)Scripts$' --output-on-failure
```

The two live integration suites were correctly skipped in the owner's full run.
This correction does not authorize live GUI testing or touch any database.

From the repository root:

```bash
bash scripts/phase12/01-build.sh
```

Defaults to build on Linux, build-ucrt64 on Windows. Optional `--build-dir PATH`
accepts a path with spaces. It configures both `TASKCOMPASS_ENABLE_DB_TESTS=OFF` and
`TASKCOMPASS_ENABLE_PHASE12_DB_TESTS=OFF`, then builds and runs CTest. The two live
suites must report **Skipped**, not a successful live test. Source-only GUI/model
tests use offscreen fake sessions, not the production database.

Equivalent explicit commands:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
   -DTASKCOMPASS_ENABLE_DB_TESTS=OFF -DTASKCOMPASS_ENABLE_PHASE12_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Windows: use `build-ucrt64` and the existing UCRT64 CMake prefix/toolchain setup.
The wrapper sets CMAKE_PREFIX_PATH from `/ucrt64`. Windows Phase 1.2 compilation
passed. The two initial offline failures were resolved by the focused retest
(2026-11-16, owner-reported). That focused regression command is:

```bash
ctest --test-dir build-ucrt64 -R '^(DatabaseSchemaTests|Phase12ValidationScripts)$' --output-on-failure
```

Schema tooling explicitly reads/writes UTF-8; no global locale/Python mode change
is required. Linux socket/sudo positive mock fixtures run only on Linux; Windows
still exercises TCP workflows, safety gates and rejection of socket options. A
SKIP message for those Linux-only fixtures does not skip the whole shell suite.
The two live integration executables still skip in ordinary CTest. Windows offline
evidence combines the original run with the focused two-test pass. Windows isolated
live integration also passed (owner-reported); development upgrades remain separately gated.

## 2 — Review the source plan (safe now)

```bash
bash scripts/phase12/02-review-plan.sh
bash scripts/phase12/03-setup-test.sh --print-plan
bash scripts/phase12/04-integration-test.sh --print-plan
python3 docs/diagrams/GenerateDatabaseSchema.py --check
```

Previews print placeholder provisioning SQL and actual 001/002 source, hashes and
suite scope. No executable SQL, client connection or credential prompt runs. The
nine-table diagram describes cumulative source, not a live deployment. Without
arguments, setup/test wrappers print help only. The build script is the exception:
its default action is an offline build/test run.

## 3 — Provision a NEW Phase 1.2 test target (approval required)

**Linux and Windows provisioning completed (owner-reported).** Approve
this separately on each intended host; do not rerun setup on the existing
accounts/databases. The fixed target is:

| Setting | Value |
| --- | --- |
| Host / port | 127.0.0.1 / 3306, TCP |
| Database | taskcompass_phase12_test |
| User / host | taskcompass_phase12_test@127.0.0.1 |
| Encoding | utf8mb4 / utf8mb4_unicode_ci |
| Privileges | SELECT, INSERT, UPDATE, CREATE, DROP, ALTER on that exact database only |

After explicit provisioning approval:

```bash
bash scripts/phase12/03-setup-test.sh --apply --approved
```

Choose administrator authentication to match the server; no root authentication
or password change is needed for this helper.

On CentOS with socket-authenticated MariaDB root, after Stage 1A approval use:

```bash
bash scripts/phase12/03-setup-test.sh --apply --approved --admin-sudo
```

Run this as your normal Linux user. The script validates sudo authorization, then
uses `sudo -n` for **only the MariaDB client**, with `--protocol=SOCKET --user=root`.
Sudo may ask for your **Linux login password**; there is no MariaDB administrator
password prompt. Do not run the whole repository script under sudo. If the current
OS account already has socket-login authority, `--admin-socket` uses it directly
without sudo. Neither socket mode supplies MYSQL_PWD to the client. Neither falls
back to TCP/password login on failure. Sudo denial/expiry stops the operation.

Windows/UCRT64 keeps the original TCP/password command above. `--admin-sudo` and
`--admin-socket` are rejected there. Linux administrators who use TCP passwords
can also keep the original default. The new **test account** remains the same
TCP account at 127.0.0.1:3306 in every case; only administrator transport differs.

Optional `--client '/path with spaces/mariadb.exe'` and `--admin-user NAME` remain
available. UCRT64 auto-detects MariaDB's Program Files client if PATH lacks it;
multiple candidates require `--client`. Type the exact prompted phrase
`CREATE taskcompass_phase12_test`. Then enter and repeat a **distinct 12+ character
password for taskcompass_phase12_test** from your password manager. This is a new
test-account password, not the Linux/sudo or MariaDB administrator password.

The helper reports hostname/version/port/platform, rejects an unapproved server
series/port, then checks the proposed database and any same-name user do not exist.
It refuses to overwrite/reuse/repair them. Password mismatch/cancellation before
provisioning submits no DDL. CREATE/USER/GRANT is not atomic: if an execution error
occurs, stop and preserve evidence; do not rerun expecting repair or auto-cleanup.
Nothing is saved to an env/credential file. Do not put passwords in command lines,
Git, shell history or logs. In TCP administrator mode, MYSQL_PWD is scoped to the
client child environment; socket modes remove it, including inherited values;
MariaDB 13 client warnings may still appear, redacted. TLS verification is unchanged.

No migrations or tests run in this step. Existing diagnostic, runtime, migration
and Phase 1.1 integration accounts/grants are unchanged.

### Linux socket connection error before provisioning

If the first administrator query reports `ERROR 2002 ... mysql.sock (2)`, the
socket path does not exist. Check the service before retrying:

```bash
systemctl is-active mariadb
```

If it reports `inactive`, start the existing service, then check again:

```bash
sudo systemctl start mariadb
systemctl is-active mariadb
```

Expect `active`. Then rerun the approved Stage 1A command with `--admin-sudo`.
Failure on the first connection occurs before CREATE DATABASE/USER/GRANT, so that
attempt performed no provisioning. If service startup fails, inspect
`systemctl status mariadb --no-pager` and stop; do not initialize a new data directory
or change authentication. If the service is active but the socket is still absent,
review its actual socket configuration before retrying; do not assume a password
reset or TCP fallback is needed. The helper does not start services automatically.

## 4 — Run disposable Phase 1.2 integration fixtures (separate approval required)

**Corrected Linux and Windows suites passed (owner-reported).** These are the reusable commands, not authorization to repeat recovery
or run on another host. After provisioning, report/review its result before
obtaining approval to run the suite. Then:

```bash
bash scripts/phase12/04-integration-test.sh --approved
```

For the CentOS socket-authenticated administrator, after **Stage 1B approval** use:

```bash
bash scripts/phase12/04-integration-test.sh --approved --admin-sudo
```

Optional `--client PATH` and `--build-dir PATH` are supported. Type exactly
`TEST taskcompass_phase12_test WITH RUN-OWNED RESET`. The administrator preflight
(catalog/grant/role/PUBLIC checks) is read-only. With --admin-sudo, sudo may request
your OS password; only the administrator client uses sudo/socket. An OS account
already authorized for direct socket login can use --admin-socket. Windows keeps
the original TCP/password command and rejects socket options. Default TCP mode
prompts for the MariaDB administrator password.

Enter the **taskcompass_phase12_test password you created in Stage 1A** at the test
password prompt. The test executable always runs as your normal OS user using the
fixed TCP test account, regardless of administrator authentication method.
The test password is exported only to the test subprocess; it is unset on exit.
Conflicting pre-existing TASKCOMPASS_PHASE12_TEST_DB_* settings stop the wrapper.
It sets HOST=127.0.0.1, PORT=3306, NAME/USER=taskcompass_phase12_test and ISOLATED=1.
It does not source or rewrite the old TASKCOMPASS_TEST_DB_* configuration.

Preflight requires approved charset/collation, absolutely no tables/views/routines/
triggers/events, exact six direct privileges, no extra global/table/column rights,
no explicit roles, and no applicable PUBLIC write/DDL rights. Unrelated PUBLIC
`test`/`test_%` grants are allowed. Unsupported grant syntax/roles stop for review.
Only after preflight does it run `OrganizationIntegrationTests --run --allow-reset`.
The executable independently checks configured/authenticated identity, FK/CHECK
settings, empty-start catalog, case mode and advisory locks before schema work.

The suite creates synthetic version-1 records, rehearses migration 002 (including
12 deliberately injected interruption points), validates preservation, legal/
illegal ownership, typed history/commands, transaction rollback, restart and
separate-connection lease timeout/loss/conflicting moves. Expected injected failure
fixtures may drop only known run-owned tables after empty-start proof. The fixture
must actually reach the injected fault; unexpected earlier failure preserves state.
Successful execution leaves the **new** schema empty. Nonempty/pre-existing schema
or any unexpected failure stops without automatic reset, resume or repair. A failed
run needs a separately reviewed recovery plan; never remove it just to rerun.

Integration exit 0 is success, 1 is failure, 77 is skipped (not a pass). Record full
nonsecret output, server/platform versions and exit code in the milestone. Both
MariaDB 10.5 and 13 must pass before any development upgrade is considered.

### Unexpected migration failure: preserve and inspect

An unexpected failure stops the suite and preserves the new test schema. Do not
rerun setup, rerun the suite, reset tables or apply migrations to repair it. The
2026-11-16 Linux failure exposed a source bug in counting trailing whitespace as a
DDL statement; the fix is offline-tested but the retained database still needs
review before cleanup/retry approval.

For the socket-authenticated Linux administrator, this command executes **read-only
inspection SQL** and prints metadata/counts without task text or credentials:

```bash
sudo mariadb --no-defaults --protocol=SOCKET --user=root \
   taskcompass_phase12_test < scripts/phase12/inspect-failure.sql
```

Share its nonsecret output. It lists server/database identity, tables/types/engines,
migration metadata and original-table row counts. If any query fails, share that
error and stop. It does not query an absent MigrationAttempt table or modify any
objects. The expected retained baseline for the statement-count failure is five
schema-1 tables, migration row 1, Dataset count 1 and Task/CommandRecord/LocalChange
counts zero; this is a source-based expectation pending actual inspection. Any
different state requires review. Concrete cleanup SQL and permission to rerun must
be approved separately after the retained state is reviewed.

## Later gates — not authorized by steps 3/4

Do not launch current TaskCompass against schema 1; it requires verified schema 2
and runtime access to the new tables. No GUI auto-migration or dual-version writes.
A development upgrade needs separate baseline/grant review, a new verified private
backup, exact migration SQL approval, legacy-value comparisons and recovery plan.
Then approve minimum runtime grant additions/migrator revocation, interactive GUI
and restart checks, and a nine-table backup/restore rehearsal on separately approved
empty targets. Preserve every prior artifact and restored database. The milestone
contains the proposed SQL/recovery framework; it does not authorize execution.

### B7 populated-snapshot and development acceptance review package

Use [Phase1.2B7.md](Phase1.2B7.md) for the proposed numbered platform-specific
preflight, fresh private schema-1 capture, populated recovery/narrow-role rehearsal,
controlled development upgrade, GUI and schema-2 backup/restore procedures.
The initial B7.1 approval is now recorded as passed on both platforms. B7.2 is
approved for owner-only capture using the [four numbered scripts](Phase1.2B72.md);
live capture results are pending. Later grants, restores and migration remain
separately gated. Approvals/results remain in the milestone.
Do not rerun disposable integration tests on a populated clone or reuse retained
taskcompass_integration/Phase 1.1 artifacts as scratch space.


## Focused usability retest — 2026-11-18

The owner reports GUI functional testing has gone well; this is not final acceptance.
Retest the refinements in [Interactions](Interactions.md#phase-12-usability-refinement--2026-11-18)
on Linux first, then Windows after Linux review. Use only the existing QA prefix
and records. Preserve all originals and previous checkpoints. No reset/cleanup.

1. Compare + and right-click creation menus for All Tasks, Inbox, pillar,
   subpillar, project, subproject, objective and visible archived entity. New Task
   is first and New Pillar last in the creation section; only legal children appear.
2. Check the navigation separator, every entity's type label, tooltip and selected
   details, and alphabetical sibling order within type groups (including mixed
   case and duplicate names). Refresh/reload and verify selection/expansion.
3. Open New Task under a project/objective and check its default owner. Choose
   Independent as an alternative; pillars never become task owners. Cancel editors
   when observation alone suffices.
4. On QA organizations only, drop onto a valid parent; inspect the existing Move
   destination and subtree/archive preview. First cancel, then complete a move.
   Undo/redo and verify unchanged task UUIDs and derived paths. Try invalid cyclic,
   pillar-to-project, objective-target, unchanged-parent and between-row drops;
   none must write. Archived destinations retain the explicit warning. Root
   promotion uses Move, not a row-position drop. Objective view disables dragging.
5. Verify Show Archived always starts off, including after closing with it on.
   This supersedes saved-preference restoration. Existing archived records remain
   archived and hidden initially; enable the filter to observe them. Other UI
   preferences retain their existing behavior. Complete normal before/after-restart
   B7.7 checkpoints as documented.

Offline coverage lives in OrganizationDesktopTests: shared menu content, all
selected-owner defaults, type roles, grouped sorting, disabled separator, session-only
archive defaults, refresh state, stale/invalid drag rejection, cancellation,
command-backed move and undo/redo with task preservation. Native platform pointer
interaction and visual quality still require owner observation. Use the normal
B7.7 launch/checkpoint scripts; do not rerun preflight on an existing QA run.


### Tree count/icon retest

Check the Pillars separator and names with counts (including zero), without verbose
type prefixes. On prefix-qualified QA data only, verify objective, direct project,
subproject, subpillar and ancestor counts after create/complete/reopen, ownership
changes, moves, archive/restore and undo/redo. Completed/tombstoned/effectively
archived tasks never count, even with Show Archived/Completed enabled. Search and
scope changes must not change these totals. Check alphabetical order independently
of counts and selection/expansion after updates. Observe distinct pillar/project/
objective icons on Linux/Windows, light/dark themes and available HiDPI; inspect
full type/path/UUID tooltips. Enable Show Archived, close and reopen: it starts off
and hides archived entities; re-enable only for observation. No original-row writes,
settings reset, QA cleanup, migration or privilege change is needed.

## Windows offline fixture retest — 2026-11-18

The owner-reported nine failures on Qt6.12.0 after a successful build are recorded
in the milestone. These tests use mocked executables/temporary synthetic evidence;
MariaDB being stopped is not their prerequisite. Explicit UTF-8 report reads,
fixture platform state and LF output have been corrected/hardened, with improved
captured-failure diagnostics. Native Windows confirmation remains pending.

After obtaining corrected sources, run in UCRT64:

```bash
ctest --test-dir build-ucrt64 --rerun-failed --output-on-failure
```

Stop and share remaining offline fixture failures; do not repair databases or alter
backup evidence. Resume the Windows checklist only when its build gate is green.


## B7.8 populated schema-2 backup package

Guides: [overview/safety/evidence](Phase1.2B78.md), [Linux](Phase1.2B78Linux.md),
[Windows UCRT64](Phase1.2B78Windows.md). New CTest entries are entirely offline:

```bash
ctest --test-dir build -R '^Phase12Schema2Backup(Tests|Scripts)$' --output-on-failure
```

Use build-ucrt64 on Windows. Tests use synthetic temporary snapshots and fake
clients/dump/verifier/admin ACL tools, never real databases or preserved captures.
They cover exact Unicode/escaping/historical payload values, nine-table completeness,
LF/CRLF dump input, extended INSERT/constraints, complete health rules, narrow Linux
exception reporting, platform/UUID/grant/writer refusal, outside-repository/fresh
private paths, Windows ACL creation failure, interrupted/empty/failed/unsafe/drifting
dumps, checksums/tampering/private-review gates and no repeated sealing. Existing
fake mutation counters must not increase during B7.8. The diagnostic password is
checked before SQL redirection; pipeline stdin must never be interpreted as a password.

Live testing is not part of this package preparation. Linux02 read-only preflight
approval is the next checkpoint; capture03 and offline sealing04 require their
separate approvals. Windows follows only after Linux results/private review and
independent authorization. An offline mock pass does not establish native Windows
permissions/tooling behavior. No reset-capable integration test is run against
any development, retained rehearsal or integration database.


Windows B7.8 ACL policy regression (2026-11-20):
`Phase12BackupAclPolicyTests` is registered only on native Windows. It uses
in-memory ACLs and mocked directory metadata, with no database or ACL writes.
It verifies historical private inheritance through a protected capture anchor
while rejecting extra rights, wrong owners, unsafe paths and unprotected new
snapshots. Linux shell fixtures verify routing and failure propagation but do
not establish native Windows ACL compatibility. See the
[Windows B7.8 procedure](Phase1.2B78Windows.md) for the direct offline command
and the already authorized read-only stage02 retry. Native result is pending.


## B7.9 isolated schema-2 restore preparation

See [B7.9 operator guide](Phase1.2B79.md) for fixed Linux/Windows snapshot paths,
five independent approvals, exact commands, new private evidence and recovery
boundaries. `Phase12Schema2RestoreTests` uses synthetic nine-table snapshots,
in-memory fake SQL sessions and isolated fake child processes only. Missing mocks
fail rather than falling back to a real service. It exercises both simulated
platforms, UTF-8/CRLF/paths with spaces, all five successful stages, source/seal
corruption, target/host conflicts, incorrect grants/PUBLIC scopes, partial import,
interruptions, data/DDL/health/object/cross-schema failures, persisted attempt
markers, redacted diagnostics and no repeat mutations/cleanup. Native Windows
CLI/PowerShell integration remains pending; these mocks are not that evidence.

```bash
python3 tests/Phase12Schema2RestoreTests.py
ctest --test-dir build -R '^Phase12Schema2RestoreTests$' --output-on-failure
```

Use build-ucrt64 on Windows. Do not enable reset-capable live integration tests
against development/rehearsal/restore targets. Existing B7.8 sealed evidence and
failed attempts remain untouched. Linux protected-task and missing restart
comparison exceptions remain unresolved for B7.10 regardless of restore results.
