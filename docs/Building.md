# Phase 0.1 development guide

This is a **desktop technology prototype**, not the task manager. It uses C++20,
Qt 6 Widgets, Qt SQL and CMake. Tasks, statuses, priorities and navigation links are
disposable presentation fixtures. Only window geometry, splitter position and
navigation visibility persist through QSettings. No TaskCompass tables, migrations,
application records, shared domain implementation or synchronization exist.

Phase 0.2 added documentation and GPL-3.0-or-later licensing only; the following
connectivity-validation follow-up permits an isolated empty development database. The prototype
still has sample text priorities, a sample status editor and fixture filter tags;
it has no production Worked Today/Done for Today/Completed controls, contexts,
undo/redo, ordering persistence, date-filter engine or custom fields. Do not use its
sample behavior as the production contract. See [Interactions.md](Interactions.md)
and [Licensing.md](Licensing.md) for the designs and redistribution review. Build
and test commands below are unchanged; outstanding 0.1 checks remain pending.

## CentOS Stream 9 prerequisites and observed environment

Inspection/validation: 2026-11-10.

| Component | Initially found | Validated after user-installed prerequisites |
| --- | --- | --- |
| Compiler | GCC/G++ 11.5.0 | C++20 Debug build passes |
| CMake | 3.31.8 | Configure/generate passes |
| Ninja | 1.10.2 | Build passes |
| Qt 6 Core/Widgets/SQL/Test and development tools | Missing | Qt 6.10.3 from qt6-qtbase-devel and dependencies |
| Qt 6 MariaDB driver | Missing; Qt 5 mysql plugin was installed | qt6-qtbase-mysql 6.10.3; QMYSQL available and loadable |
| MariaDB Connector/C | Runtime 3.2.6, development headers missing | mariadb-connector-c-devel 3.2.6; QMYSQL resolves libmariadb.so.3 |
| MariaDB client/server packages | 10.5.29 installed | No server connection attempted; no isolated database configuration supplied |
| Display | DISPLAY=:1, X11 socket present | Actual-display startup/automatic close passed; interactive usability review pending |
| Windows toolchain | Unavailable in this environment | Build and runtime validation pending |

The initial CMake failure was missing `Qt6Config.cmake`, not a C++ compilation
failure. The attempted sudo install could not authenticate; the user subsequently
confirmed installation. No application code invokes a package manager.

For a machine missing prerequisites, review and run these commands yourself or
approve installation first:

```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtbase-mysql mariadb-connector-c-devel
```

Package availability depends on enabled distribution repositories; Qt 6 packages
may require EPEL. Do not enable a new repository automatically. This project does
not require a server installation to build or run the GUI. A server is needed only
for an explicitly requested connectivity test. The project declares CMake 3.21+
and Qt 6.2+; only the exact versions above have been tested. Current filtering APIs
are used on Qt 6.10+, with an older-Qt fallback. GCC 11's supported C++20 subset is
sufficient for this prototype; no claim about every C++20 library feature is made.

## Configure, build, run, test, clean

From the repository root, using any terminal and VIM:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
./build/TaskCompassPrototype
ctest --test-dir build --output-on-failure
cmake --build build --target clean
cmake --build build --parallel 2
```

For an alternate Qt installation add `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64`
to configure. For a build without automated tests, use `-DBUILD_TESTING=OFF`;
Qt Test is then unnecessary. No IDE, QML runtime, ORM or third-party testing library
is required. CMake builds three executables when tests are enabled:
`TaskCompassPrototype`, `TaskCompassDbDiagnostic`, and `PrototypeTests`.

Normal GUI execution needs an accessible desktop display. A short startup check:

```bash
./build/TaskCompassPrototype --smoke-test
QT_QPA_PLATFORM=offscreen ./build/TaskCompassPrototype --smoke-test
```

Smoke mode closes after 500 ms and uses temporary QSettings, leaving normal layout
preferences untouched. CTest uses the offscreen plugin and temporary test settings,
so it can run without a display and does not change your layout. Offscreen
`propagateSizeHints` warnings are platform-plugin limitations, not test failures.
A missing offscreen platform plugin is a runtime dependency failure, distinct from
compilation failure. Offscreen testing does not establish visual usability.

The main window uses QMainWindow, QSplitter, QTreeView and QTableView. Header clicks
sort a proxy; source insertion order remains unchanged. Drag header boundaries to
resize columns; use arrow keys to select rows. Double-click a row or use Ctrl+E to
edit. Ctrl+N adds a sample task, Ctrl+F focuses filtering, Ctrl+Shift+N toggles the
navigation panel, Ctrl+Q exits, and F1 opens About. Filtering is a case-insensitive
literal substring across all displayed columns, intersected with navigation tags.

The pillar tree demonstrates Family → Household project → Family outing objective;
Council → Service focus objective directly under the pillar; and Learning → Study
subpillar → TaskCompass project → Desktop notes subproject / Complete desktop
prototype objective. Household and Desktop notes expose direct project tasks;
objectives expose their tasks; Inbox exposes an independent capture.

The alternate Objectives and Sprints branches reuse the same fixture keys and task
rows. Complete desktop prototype appears in November and December 2026 selections
without changing its TaskCompass ownership context. These are static presentation
fixtures, not planning-period records, task scheduling or production ownership
logic. The Organization context column and ancestor tags are hand-authored display
values; the future domain will derive those paths from the documented parents.
New samples are always independent inbox captures, regardless of the selected view;
the UI returns to All sample tasks to show the new row. The dialog does not edit
ownership, priorities or context labels. There is no manual drag ordering, Done for
Today operation, recurrence, planning or lifecycle enforcement yet.

Normal layout settings are stored in `TaskCompass/DesktopPrototype.ini` under Qt's
platform user configuration directory (typically `~/.config` on Linux and the
user's application configuration directory on Windows). No task data or credentials
are stored there. A settings write failure does not prevent closing the prototype.

## Scripted development environment and database validation

The repository now includes Bash helpers for **Linux and MSYS2 UCRT64 on Windows**.
They require Bash and a MariaDB command-line client; no Python, new libraries,
package installation or IDE is required. Run from the repository root. Keep shell
scripts with LF line endings; `.gitattributes` enforces this for Git checkouts.
These scripts are development diagnostics/provisioning, not application persistence
or migrations. They never install packages, start services or create application
tables. A running local MariaDB service on port 3306 remains a prerequisite.

### Windows/UCRT64 quick start

Open the **UCRT64 terminal**, then:

```bash
source scripts/dev-environment.sh
bash scripts/dev-database.sh setup
```

The environment helper sets the existing TASKCOMPASS_DB_* non-secret settings,
puts UCRT64 tools first in PATH and sets CMAKE_PREFIX_PATH for the UCRT64 kit. It
changes only this shell, not Windows settings or startup files. Use a fresh
`build-ucrt64` directory for this kit. Build before setup's automatic Qt check:

```bash
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ucrt64 --parallel 2
ctest --test-dir build-ucrt64 --output-on-failure
```

If the client is not on PATH, setup searches standard `C:/Program Files/MariaDB*/bin`
installations. With multiple installations, specify which executable to use. For
example, adjusting the version/path to your actual installation:

```bash
bash scripts/dev-database.sh setup \
   --client '/c/Program Files/MariaDB 11.4/bin/mariadb.exe' \
   --build-dir build-ucrt64
```

Setup prompts for the **MariaDB administrator password** and then a new development
password twice. Create/save that new password in your password manager first;
at least 20 characters are required. The script does not generate a hidden password
or write any password file. Setup inspects database names, target defaults, account
hosts, grants and roles first; it **stops without changing anything if either the
database or any same-name account exists**. A stopped setup is not a password reset.
For a newly created account, it immediately runs CLI and Qt validation with the
password still in memory. If that later check fails, inspect the partial result;
do not expect a repeat setup to overwrite the created account.

For an existing development database, use **test**, not setup:

```bash
bash scripts/dev-database.sh test --build-dir build-ucrt64
```

It prompts for the development password, checks TCP authentication, selected schema,
CURRENT_USER host matching, empty tables, privilege metadata and direct grants,
runs the existing Qt diagnostic with --connect, and checks that tables remain empty.
Use the same --client option if client discovery needs help. For a multi-config
build, pass the directory actually containing the executable, e.g. build/Debug.

### Linux quick start

```bash
source scripts/dev-environment.sh
# Only when the service needs to be started, by an authorized local operator:
sudo systemctl start mariadb
# Fresh database/account only; local root socket authentication:
sudo bash scripts/dev-database.sh setup --admin-socket --client /usr/bin/mariadb
# Existing database; restricted account, no sudo required:
bash scripts/dev-database.sh test --build-dir build
```

If socket administration needs review before creation:

```bash
sudo bash scripts/dev-database.sh inspect --admin-socket --client /usr/bin/mariadb
```

On Windows use `inspect` without --admin-socket; it prompts for the database admin
password and uses TCP. --admin-user selects a different authorized administrator.
No administrator password is requested during `test`. The GUI prototype never
uses administrator credentials or connects automatically.

### Optional diagnostic environment

The test action supplies variables only to its child diagnostic. If you want to
run the diagnostic manually or keep settings in your current shell:

```bash
source scripts/dev-environment.sh --password
./build-ucrt64/TaskCompassDbDiagnostic.exe --connect
clearTaskCompassDatabasePassword
```

Use `./build/TaskCompassDbDiagnostic --connect` on Linux. The hidden prompt loads
TASKCOMPASS_DB_PASSWORD without putting its value in shell history. The helper
sets TASKCOMPASS_DB_HOST=127.0.0.1, PORT=3306, NAME=taskcompass_dev,
USER=taskcompass_dev and ISOLATED=1 under their established full variable names.
Source with --no-password for noninteractive environment setup. Without --connect,
the diagnostic still lists drivers and skips networking. Never use shell tracing
with real secrets; the source helper refuses it. Clear the password when finished.

### Scope, privileges and credential handling

The target is fixed to **taskcompass_dev@127.0.0.1:3306**, with
**utf8mb4 / utf8mb4_unicode_ci**. These scripts do not repurpose existing application
configuration or credentials. No password appears in process arguments or logs;
passwords are held in shell memory and short-lived child environments (MYSQL_PWD
for the CLI, existing TASKCOMPASS_DB_PASSWORD for Qt). Administrators/processes with
sufficient access may inspect environments; this is not encrypted credential
storage. Nothing is persisted by the helpers. They neither read nor delete the old
private `/tmp/taskcompass-dev-*/client.cnf` from earlier manual provisioning; manage
that file separately and retain its password privately if still needed.

Fresh setup executes only database/account/grant statements, with password SQL
supplied on stdin, escaped for a session-only NO_BACKSLASH_ESCAPES SQL mode:

```sql
CREATE DATABASE taskcompass_dev CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'taskcompass_dev'@'127.0.0.1' IDENTIFIED BY '<private-password>';
GRANT SELECT ON `taskcompass\_dev`.* TO 'taskcompass_dev'@'127.0.0.1';
```

CREATE deliberately fails on a concurrent create instead of granting to an unknown
account. The escaped underscore makes the grant exact rather than a database-name
pattern ([MariaDB GRANT](https://mariadb.com/docs/server/reference/sql-statements/account-management-sql-statements/grant)).
There is no GRANT OPTION, table creation or automatic privilege repair. Validation
inspects grants without displaying authentication hashes and makes no destructive
CREATE/DROP permission test. For preexisting accounts, administrator `inspect` also
checks same-name host accounts and roles; do not infer complete server-wide security
policy from restricted-user metadata alone.

| Purpose | Account policy |
| --- | --- |
| Current validation | Empty taskcompass_dev, SELECT only; no application schema or migrations. |
| Future development persistence | Add only explicitly authorized privileges for the milestone; scripts do not upgrade existing users. |
| Future production runtime | Separate database/credentials, least-privilege DML; not this development account or root. |
| Future migrations | Separate operator/migration credentials with scoped DDL; none created here. |

### Troubleshooting and manual fallback

Service/listener problems: inspect Windows Services or use `systemctl is-active
mariadb` / `ss -ltn 'sport = :3306'` on Linux. Scripts report the failure and do not
start/reconfigure a server or open a firewall. Install MariaDB Server separately if
only client libraries are present. For access denied, check password and the exact
127.0.0.1 account; unknown database means provisioning/name needs review. Use
`bash scripts/dev-database.sh inspect ...` with authorized admin access before
changing anything. An existing unescaped wildcard grant is rejected for review,
not automatically revoked/replaced. Partially completed setup requires inspection.

For direct client testing (use the full executable path on Windows if needed):

```bash
mariadb --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_dev --password taskcompass_dev
```

Run `SELECT VERSION(); SELECT DATABASE(); SELECT CURRENT_USER(); SHOW TABLES;`.
No tables are expected. `mysql` is an equivalent client command when available.

QMYSQL issues are separate from database credentials. CentOS needs qt6-qtbase-mysql,
not a Qt 5 plugin. Inspect `ldd /usr/lib64/qt6/plugins/sqldrivers/libqsqlmysql.so` or
use QT_DEBUG_PLUGINS=1 for plugin/library errors, without sharing secrets in logs.
Windows Qt, connector, compiler and architecture must match; keep the UCRT64 kit
consistent. See [Qt SQL drivers](https://doc.qt.io/qt-6/sql-driver.html). A working CLI
with failed Qt open calls for plugin/configuration investigation, not architecture
changes or global grants. The diagnostic's existing exit codes remain documented
in its source: 0 skipped/success, 2 missing plugin, 3 bad configuration, 4 connection
failure, 5 query failure. Read the text to distinguish skipped from connected.

### Helper validation status

Shell syntax and six mock-client test groups pass on Linux, including paths with
spaces, native-client CRLF output, exact grant SQL, quoting a password containing
quote/backslash, rejection of existing resources/nonempty schemas/extra roles,
redaction and environment cleanup. CTest now includes DevDatabaseScripts when Bash
is available; these tests create no real databases. No extra test library is used.
The normal C++ build and all three CTest targets passed. A live Linux helper run
failed before authentication with ERROR 2002 (HY000), unable to reach
127.0.0.1:3306; it made no database changes. Windows script execution and successful
live connectivity remain pending. No server/account was provisioned by this script
implementation task, and Phase 1.1 has not started.

## Tests and current results

`ctest --test-dir build --output-on-failure` runs:

- `PrototypeTests`: eight behavior cases covering model structure/notifications,
  insert/update validation, proxy sorting without changing source order, combined
  navigation/text filtering, consistent project/objective/sprint views of the same
  fixture rows, independent capture, accepted/cancelled dialog edits, action-driven editing
  of a sorted row and creation, QSettings layout restoration/navigation visibility,
  and diagnostics with no connection or incomplete configuration. Qt's model tester
  checks source/proxy model contracts. Diagnostic subprocess tests strip all
  TASKCOMPASS_DB variables and never contact a server.
- `DevDatabaseScripts` (when Bash is available): isolated fake-client tests of the
  database/environment helpers; no server, database or real credentials needed.
- `GuiStartup`: starts the actual application offscreen and closes it cleanly with
  temporary settings.

Actual commands/results and pending acceptance checks are recorded in
[Roadmap.md](Roadmap.md). No successful live helper connection result or Windows build is available
from this helper follow-up; provisioning/availability must be reviewed separately. These checks remain pending;
Phase 0.1 must not be called complete yet.

## Windows (pending validation)

MSYS2 UCRT64 is a supported development choice for this prototype design. Use its
Qt/compiler/CMake/Ninja kit consistently, source scripts/dev-environment.sh and use
`build-ucrt64` as above. The Bash database scripts run in that terminal and can call
the separately installed native Windows MariaDB client by full path. MSVC below is
an alternative, not a requirement. Neither kit has been compiled by this Linux
agent; reported Windows QMYSQL loading is user evidence, not a server connection.

Use a Windows machine/VM with CMake 3.21+, Ninja, a C++20 compiler (for example
Visual Studio 2022 **Build Tools**, no editor required), and a matching Qt 6 desktop
kit with Core, Gui, Widgets, SQL and Test. The Qt kit, compiler and MariaDB
Connector/C must agree on architecture and ABI; the commands below assume MSVC
x64, not a MinGW kit. QMYSQL is not necessarily included in a Qt SDK: obtain/build
the matching plugin and distribute its MariaDB Connector/C runtime dependencies.
See the [Qt SQL driver instructions](https://doc.qt.io/qt-6/sql-driver.html).

Run from an x64 compiler developer terminal (PowerShell example; adjust Qt version):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=C:/Qt/6.x/msvc2022_64
cmake --build build --parallel 2
.\build\TaskCompassPrototype.exe
ctest --test-dir build --output-on-failure
.\build\TaskCompassDbDiagnostic.exe
cmake --build build --target clean
cmake --build build --parallel 2
```

If using the Visual Studio multi-configuration CMake generator, omit
`CMAKE_BUILD_TYPE` and use `cmake --build build --config Debug`,
`ctest --test-dir build -C Debug --output-on-failure`, and executables under
`build/Debug/`. Use a fresh build directory when changing generator or Qt kit.
This prototype uses a console-subsystem GUI executable, so diagnostics remain
visible during evaluation. MSVC compilation is configured for UTF-8 source text.

For a deployed smoke check, build Release, run the matching Qt kit's
`windeployqt` on the GUI executable, then verify the Qt SQL diagnostic separately
with its `sqldrivers/qsqlmysql.dll` and MariaDB runtime on the DLL search path.
Match debug/release DLL variants; do not copy Linux plugins to Windows. The GUI
has no SQL dependency and needs no MariaDB installation; a Windows-hosted database
requires a separately configured MariaDB service. No installer is supplied yet.

Use PowerShell `$env:TASKCOMPASS_DB_HOST`, `$env:TASKCOMPASS_DB_NAME`,
`$env:TASKCOMPASS_DB_USER`, `$env:TASKCOMPASS_DB_PORT` and
`$env:TASKCOMPASS_DB_ISOLATED='1'` for an existing isolated database. Set its password
through an appropriate secure local process and remove the environment value
after running `.\build\TaskCompassDbDiagnostic.exe --connect`.

Windows compilation, plugin deployment, layout/shortcuts and connectivity must be
verified on Windows before marking the platform criterion complete. Linux success
is not evidence of a verified Windows build.
