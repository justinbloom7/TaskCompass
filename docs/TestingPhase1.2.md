<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 source review and isolated validation

The [milestone record](milestones/Phase1.2.md) is authoritative for approvals and
actual results. **Only source/offline work is authorized so far.** Do not execute
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
The wrapper sets CMAKE_PREFIX_PATH from `/ucrt64`. Windows Phase 1.2 results remain
pending; accepted Phase 1.1 Windows results do not validate this new source.

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

**Not authorized or executed yet.** Approve this separately on each intended host:
CentOS/MariaDB 10.5 and Windows/MariaDB 13. The fixed target is:

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

Optional `--client '/path with spaces/mariadb.exe'`, `--admin-user NAME`, or Linux
`--admin-socket` use the existing provisioning interface. UCRT64 auto-detects
MariaDB's Program Files client if PATH lacks it; multiple candidates require
`--client`. Type the exact prompted phrase `CREATE taskcompass_phase12_test`.
The administrator password is only for setup/inspection. Then enter and repeat a
**distinct 20+ character test-account password** from your password manager.

The helper reports hostname/version/port/platform, rejects an unapproved server
series/port, then checks the proposed database and any same-name user do not exist.
It refuses to overwrite/reuse/repair them. Password mismatch/cancellation before
provisioning submits no DDL. CREATE/USER/GRANT is not atomic: if an execution error
occurs, stop and preserve evidence; do not rerun expecting repair or auto-cleanup.
Nothing is saved to an env/credential file. Do not put passwords in command lines,
Git, shell history or logs. MYSQL_PWD is scoped to the client child environment;
MariaDB 13 client warnings may still appear, redacted. TLS verification is unchanged.

No migrations or tests run in this step. Existing diagnostic, runtime, migration
and Phase 1.1 integration accounts/grants are unchanged.

## 4 — Run disposable Phase 1.2 integration fixtures (separate approval required)

**Not authorized or executed yet.** After provisioning, report/review its result
before obtaining approval to run the suite. Then:

```bash
bash scripts/phase12/04-integration-test.sh --approved
```

Optional `--client PATH` and `--build-dir PATH` are supported. Type exactly
`TEST taskcompass_phase12_test WITH RUN-OWNED RESET`. Enter the administrator
password for **read-only preflight** (catalog/grant/role/PUBLIC checks). Enter the
new test-account password when prompted. They are different accounts/passwords.
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

## Later gates — not authorized by steps 3/4

Do not launch current TaskCompass against schema 1; it requires verified schema 2
and runtime access to the new tables. No GUI auto-migration or dual-version writes.
A development upgrade needs separate baseline/grant review, a new verified private
backup, exact migration SQL approval, legacy-value comparisons and recovery plan.
Then approve minimum runtime grant additions/migrator revocation, interactive GUI
and restart checks, and a nine-table backup/restore rehearsal on separately approved
empty targets. Preserve every prior artifact and restored database. The milestone
contains the proposed SQL/recovery framework; it does not authorize execution.
