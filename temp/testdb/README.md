<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Disposable integration database helpers

These are reusable reviewed helper files in the owner's requested `temp/testdb`
location, not credentials or database storage. They target **taskcompass_integration**
and **taskcompass_test@127.0.0.1:3306** only. The database persists in MariaDB until
explicitly removed; the integration test creates/drops its own disposable tables.
No development/diagnostic account is reused or changed.

Creating these files did not authorize or execute live SQL. The exact Stage 1
operations still require the approval recorded in
[Phase1.1.md](../../docs/milestones/Phase1.1.md). Never use these helpers for Stage 2
or eEnvelopes. See [the walkthrough](../../docs/TestingPhase1.1.md) for prerequisites,
full administrator/grant/PUBLIC inspection and failure handling.

## 1. Build and inspect without database access

Linux, from the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTASKCOMPASS_ENABLE_DB_TESTS=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
bash temp/testdb/setup.sh --print-plan
```

`setup.sh` and `test.sh` without arguments print help only. --print-plan prints
placeholder SQL with no password prompt or client execution. `setup.template.sql`
is a **review template, not a directly executable script**: never edit a real
password into it or SOURCE it in MariaDB. setup.sh replaces its token in shell
memory and passes SQL on stdin, without saving a rendered copy.

## 2. Fresh provisioning — only after Stage 1 approval

First complete the milestone's administrator preflight/inherited-policy review.
On Linux with authorized root socket access:

```bash
sudo bash temp/testdb/setup.sh --apply --admin-socket --client /usr/bin/mariadb
```

For TCP administration (Linux or Windows/UCRT64):

```bash
bash temp/testdb/setup.sh --apply --client /path/to/mariadb
```

Use --admin-user NAME if the authorized administrator is not root. On Windows,
--client may be a quoted path such as
`'/c/Program Files/MariaDB 13.0/bin/mariadb.exe'`, adjusted to the installed path.
If omitted, discovery checks PATH and standard Windows installation locations;
multiple installations require an explicit choice. --admin-socket is Linux-only.
No packages/services are installed, started or reconfigured.

Type exactly `CREATE taskcompass_integration` at confirmation. TCP administration
then prompts for the database administrator password (sudo separately prompts for
Linux authorization). The script checks @@port=3306 and that the database and all
same-name accounts are absent before requesting a new integration password twice.
Choose/save a distinct strong single-line password in your password manager (20+
characters; 32+ randomly generated characters recommended). Nothing is generated
or saved secretly. Quotes/backslashes are escaped using a session-only SQL mode.

Provisioning executes only CREATE DATABASE, CREATE USER and the reviewed six-right
schema GRANT from the template. No IF NOT EXISTS, reset, password replacement,
automatic repair, application tables or migrations. Concurrent create failures
stop without granting to an unknown account. If provisioning partly succeeds,
preserve the result and inspect; do not expect rerunning it to overwrite/repair.
The script does not automatically validate every role/PUBLIC inheritance: finish
the milestone's privileged grant review before live tests. It never runs tests.
Client TLS warnings remain diagnostics; TLS verification is not disabled.

## 3. Run the approved test

When the schema is empty and its grants have been verified:

```bash
bash temp/testdb/test.sh --run --allow-reset --build-dir build
```

Windows/UCRT64 uses --build-dir build-ucrt64. Without --build-dir the runner selects
that platform default relative to the repository, so it also works from elsewhere.
Paths with spaces are supported. The script refuses conflicting TEST settings
rather than silently changing an existing target. The only executable it invokes
is MariaDbIntegrationTests[.exe], with the exact --run --allow-reset arguments.

It prompts once for the **integration account password**, stores it only in its
process environment and clears it on exit. Status 0/1/77 is preserved from the test.
The C++ guards independently verify identity, fresh empty schema and reset scope.
Failed test evidence is not automatically removed; request reviewed recovery.

## 4. Optional environment in your current shell

```bash
source temp/testdb/env.sh --no-password
# Or to prompt and export the password for manual tools:
source temp/testdb/env.sh --password
# ... run the approved diagnostic/test command ...
clearTaskCompassTestPassword
```

Only TASKCOMPASS_TEST_DB_* is configured, never TASKCOMPASS_DB_* or shell profiles.
No password means no prompt by default. env.sh must be sourced, and refuses shell
tracing. The runner uses a preloaded password if present; it cannot clear a parent
shell's environment. If you source --password, call clearTaskCompassTestPassword
in that same parent shell afterward. Do not paste credentials into export commands.

## Local files and validation

The local `.gitignore` allows only these reviewed helper files into Git and ignores
other files here to reduce accidental inclusion of logs/configuration. This is not
secure credential storage: **never save passwords here**, even in ignored files.
No helper writes a password file, rendered secret SQL or test logs.

Database-free checks:

```bash
bash -n temp/testdb/setup.sh temp/testdb/env.sh temp/testdb/test.sh tests/IntegrationDatabaseScripts.test.sh
bash tests/IntegrationDatabaseScripts.test.sh
```

The mock suite checks no-op defaults, opt-in/confirmation, existing-resource and
failed-inspection refusal, escaped SQL, password redaction, target validation,
status propagation and paths with spaces. CTest includes IntegrationDatabaseScripts
when Bash is available. No mock test connects to MariaDB. Live Linux provisioning,
Windows execution and real integration validation remain pending approval/testing.
