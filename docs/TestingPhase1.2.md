<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 source review and isolated validation

The [milestone record](milestones/Phase1.2.md) is authoritative for approvals and
actual results. **Linux Stage 1A and corrected Stage 1B passed (owner-reported);
Windows Stage 1A and Stage 1B also passed. B7.1 read-only development preflight,
including corrected Qt schema-1 verification, passed on both platforms on
2026-11-17 (owner-reported). B7.2 fresh schema-1 backup/exports are approved for
owner execution only; [numbered operator scripts](Phase1.2B72.md) are prepared.
Linux B7.2 passed after targeted completion and owner-confirmed private SQL review;
retained path/hash are in the milestone. Windows capture/review remains pending.
No live capture was performed by the agent. Development upgrades and subsequent
privilege/GUI/backup gates remain pending and require separate approval.** Do not execute
steps 3 or 4 without separate approval for that step, platform and target. None of
these steps upgrades taskcompass_dev. Preserve taskcompass_integration, all accepted
Phase 1.1 records and private backups/exports. Do not use its old reset scripts here.

Use Bash on CentOS Stream 9 or the MSYS2 **UCRT64** terminal on Windows. Prerequisites
remain in [Installation](Installation.md): C++20 compiler, Qt 6 Widgets/SQL/Test,
loadable QMYSQL and compatible MariaDB client libraries, CMake, Ninja, Bash,
coreutils (including sort/sha256sum), and MariaDB server/client for the later live
steps. No script installs packages or changes service/TLS settings. CTest uses only
fake clients/in-memory repositories unless explicitly opted into a live suite.

## 1 — Build and run offline tests (safe now)

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
