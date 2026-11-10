# Phase 0.1 development guide

This is a **desktop technology prototype**, not the task manager. It uses C++20,
Qt 6 Widgets, Qt SQL and CMake. Tasks, statuses, priorities and navigation links are
disposable presentation fixtures. Only window geometry, splitter position and
navigation visibility persist through QSettings. No TaskCompass tables, migrations,
application records, shared domain implementation or synchronization exist.

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

## Optional QMYSQL/MariaDB diagnostic

```bash
./build/TaskCompassDbDiagnostic
```

This lists Qt SQL drivers, reports QMYSQL availability and loadability, and exits
successfully **without connecting**, even if database environment variables exist.
This successful exit does not prove database connectivity. The GUI never invokes
the diagnostic and does not require MariaDB to be running.

To test an **existing, dedicated isolated test database**, configure these values
in your shell. Do not select eEnvelopes or supply its credentials. The diagnostic
never creates a database, user or table. Prefer a test user restricted to the probe
database; provisioning that database/user is a separate operator action.

```bash
export TASKCOMPASS_DB_HOST=127.0.0.1
export TASKCOMPASS_DB_PORT=3306
export TASKCOMPASS_DB_NAME=taskcompass_probe
export TASKCOMPASS_DB_USER=taskcompass_probe
export TASKCOMPASS_DB_ISOLATED=1
read -r -s -p 'Probe database password: ' TASKCOMPASS_DB_PASSWORD
export TASKCOMPASS_DB_PASSWORD
./build/TaskCompassDbDiagnostic --connect
unset TASKCOMPASS_DB_PASSWORD
```

The database name above is an example, not a pre-created database. The isolated
flag is an explicit operator assertion, not automatic detection of database
ownership. The program does not inspect checkbook configuration or inherit generic
MYSQL environment variables. Host, database and user are required; port defaults
to 3306. Password is optional for a server explicitly configured to accept that
user without one. No password command-line argument is accepted.

The probe opens QMYSQL, runs `SELECT VERSION()`, prints the server version, closes
and removes its named connection. Qt may issue session-initialization statements;
the diagnostic accesses no tables and issues no application writes. Connection,
read and write timeouts are set to five seconds; OS name resolution can take longer.
For an internet endpoint use a suitably protected network; TLS configuration is not
part of this local prototype probe. Do not commit secrets or capture environment
values in bug reports.

Exit codes: 0 = skipped or successful (read the message), 2 = QMYSQL unavailable or
unloadable on requested connection, 3 = incomplete/invalid configuration,
4 = connection failure, 5 = version-query failure. Missing-driver details:

- CentOS needs **qt6-qtbase-mysql**, not the installed Qt 5 plugin. The inspected
  plugin is `/usr/lib64/qt6/plugins/sqldrivers/libqsqlmysql.so`.
- Plugin, Qt and connector architectures/versions must match. On Linux inspect
  dependencies with `ldd /usr/lib64/qt6/plugins/sqldrivers/libqsqlmysql.so`.
- If the plugin exists but cannot load, run with `QT_DEBUG_PLUGINS=1` and examine
  library paths/dependencies. Inspect logs for sensitive information before sharing.
- A standalone SDK may require building the SQL plugin against the matching Qt
  source and MariaDB Connector/C. See [Qt SQL driver instructions](https://doc.qt.io/qt-6/sql-driver.html).

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
- `GuiStartup`: starts the actual application offscreen and closes it cleanly with
  temporary settings.

Actual commands/results and pending acceptance checks are recorded in
[Roadmap.md](Roadmap.md). No configured database, password, successful SELECT result
or Windows build is available from this session. These checks remain pending;
Phase 0.1 must not be called complete yet.

## Windows (pending validation)

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
