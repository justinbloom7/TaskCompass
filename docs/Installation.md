# Development prerequisites — Windows and Linux

This guide sets up **Phase 0.1 development and validation**, not a production
TaskCompass installation. The GUI still uses in-memory examples; MariaDB is used
only by the optional diagnostic. No production installer, schema or migrations exist.
Commands here are for the operator to review/run; the helpers do not install
packages, configure system services or require an IDE. No installation was performed
while writing this document. Build/test details: [Building.md](Building.md).

## What is needed

| Component | Purpose | Required when |
| --- | --- | --- |
| Windows x64 with MSYS2 UCRT64, or CentOS Stream 9 | Supported project development targets | Building on that platform |
| C++20 compiler | Compile prototype | Build; GCC in UCRT64/Linux, or compatible MSVC kit |
| CMake 3.21+ and Ninja | Configure/build from terminal | Build; no Qt Creator or VS Code required |
| Qt 6.2+ Core/Gui/Widgets/SQL | Desktop UI and diagnostic | Build; use a consistent compiler/Qt architecture and runtime |
| Qt Test | Automated C++ tests | Default BUILD_TESTING=ON; omit with BUILD_TESTING=OFF |
| Bash/core command-line tools | Environment/database helpers and their optional CTest target | Scripts; supplied by MSYS2 or normal CentOS installation |
| MariaDB Connector/C and Qt QMYSQL plugin | Load MariaDB driver | Live Qt database validation; headers if building the plugin |
| MariaDB command-line client | Setup/inspection/read-only CLI tests | Database helpers |
| MariaDB Server service, local TCP 3306 | Actual database to connect to | Database validation; **not required for GUI-only use** |
| Admin credentials/rights | Install packages/services, create dedicated database/user | Setup only; runtime test uses SELECT-only taskcompass_dev |
| Desktop display and Qt platform plugin | Show Widgets windows | Interactive GUI run; offscreen plugin serves automated startup tests |
| VIM or preferred terminal editor | Edit source/docs | Developer choice; not a build dependency |

A loadable QMYSQL driver proves only that its libraries load. It does not prove that
a server is installed, running, listening, or accessible with your credentials.
Use the existing development scripts after installing the prerequisites. Python,
containers, an ORM, Qt Quick and mobile SDKs are not prerequisites for this phase.
The declared minimum versions are not an assertion of testing every version;
[Building.md](Building.md) records actual observed Linux versions and pending checks.

## Windows — MSYS2 UCRT64 build environment

1. Install [MSYS2](https://www.msys2.org/) using its official Windows installer.
   The examples assume `C:\msys64`; adjust if installed elsewhere.
2. Open **MSYS2 UCRT64** from the Start menu, not plain MSYS or MINGW64.
3. Update MSYS2. If instructed to close all MSYS2 terminals, do so, reopen UCRT64,
   and repeat the update until complete. See [MSYS2 updating](https://www.msys2.org/docs/updating/).

```bash
pacman -Syu
```

Install the UCRT64 build and Qt/connector packages:

```bash
pacman -S --needed \
   mingw-w64-ucrt-x86_64-gcc \
   mingw-w64-ucrt-x86_64-cmake \
   mingw-w64-ucrt-x86_64-ninja \
   mingw-w64-ucrt-x86_64-qt6-base \
   mingw-w64-ucrt-x86_64-libmariadbclient
```

Optional editor/source-management tools: `pacman -S --needed vim git`. Git is not
required if you copy the source another way. Copy the repository without Linux
build artifacts; retain LF shell-script endings (enforced by .gitattributes).

Qt base provides the Widgets/SQL/Test modules; the MSYS2 package lists the MariaDB
client-library dependency for its MySQL/MariaDB driver. This **client library is
not a MariaDB Server installation**. Keep GCC/Qt/connector packages in UCRT64;
do not mix the MSVC Qt SDK or MINGW64 libraries into this build.
Sources: [MSYS2 environments](https://www.msys2.org/docs/environments/),
[UCRT64 Qt base package](https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qt6-base).

From the repository root, verify tools and build before database setup:

```bash
echo "$MSYSTEM"              # Expected: UCRT64
command -v g++ cmake ninja
source scripts/dev-environment.sh
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ucrt64 --parallel 2
ctest --test-dir build-ucrt64 --output-on-failure
./build-ucrt64/TaskCompassPrototype.exe
./build-ucrt64/TaskCompassDbDiagnostic.exe
```

The last command should list/load QMYSQL and **skip connection** by default. If it
loads, there is no need to rebuild QMYSQL merely to install a separate server.
For MSVC instead of UCRT64, see the alternative Windows instructions in Building.md;
use the matching compiler/Qt kit and a separate build directory. The Bash helpers
are intended for Linux/MSYS2 terminals; PowerShell itself does not interpret Bash.

## Windows — install MariaDB Server

First inspect Windows **Services** for an existing MariaDB/MySQL service and check
whether port 3306 is already in use. Reuse an authorized existing server rather than
installing a second instance on that port. Do not reset existing passwords or
upgrade an unrelated installation as part of this prototype setup.

For a new local instance:

1. Open the official [MariaDB Server downloads](https://mariadb.org/download/), choose
   a supported stable/LTS release, **Windows x86_64**, and the **MSI installer**.
   The server version need not equal the Connector/C version. Record the actual
   installed version; do not treat example directory version numbers as a requirement.
2. Run the installer with Windows administrator permission. In Custom Setup include
   **Database instance** and **Client programs**. Installing only client headers or
   connector libraries does not install a running database instance.
3. Set a strong **MariaDB root password** and save it in your password manager.
   This is a database administrator password, not your Windows password or the
   future taskcompass_dev password. Leave anonymous accounts and remote root access
   disabled for this local setup.
4. Select **Install as service**; retain the name **MariaDB** or record your chosen
   service name. Enable **TCP networking**, port **3306**. Keep normal transactional
   defaults. The helper creates the dedicated database with utf8mb4 explicitly;
   an installer checkbox labeled UTF8 is not a substitute for that database setting.
5. Finish installation. It normally starts the new service. Open **Services**
   (`Win+R`, `services.msc`) and confirm that the chosen service is **Running**.
   Start it there if stopped. Service startup may require administrator permission.

These options are described by the [official MariaDB Windows MSI guide](https://mariadb.com/docs/server/server-management/install-and-upgrade-mariadb/installing-mariadb/binary-packages/installing-mariadb-msi-packages-on-windows).
For local development, configure a new instance to bind to loopback (127.0.0.1):
if needed set `bind-address=127.0.0.1` in its `[mysqld]` section in `my.ini`, then
restart that service. Do not change a shared server's binding without reviewing
its other users. No public firewall opening or remote root account is needed for
this localhost check. Keep future internet access behind the planned application
service/API, not a public SQL port.

Optional Windows service/listener checks, in **PowerShell** (administrator elevation
is needed for Start-Service):

```powershell
Get-Service -Name MariaDB
Start-Service -Name MariaDB                 # Only if stopped; adjust service name
Get-NetTCPConnection -LocalPort 3306 -State Listen
```

Return to the **UCRT64** terminal and repository. The helper finds a standard
`C:/Program Files/MariaDB*/bin/mariadb.exe` installation, or accepts --client.
Do not add the server's bin directory ahead of the UCRT64 Qt runtime globally.
Example (replace 11.4 with your installed directory):

```bash
bash scripts/dev-database.sh setup \
   --client '/c/Program Files/MariaDB 11.4/bin/mariadb.exe' \
   --build-dir build-ucrt64
```

Enter the **MariaDB root password** when prompted, then choose/save a separate
new development password of at least 20 characters and enter it twice. Setup creates
only taskcompass_dev (utf8mb4/utf8mb4_unicode_ci) and taskcompass_dev@127.0.0.1 with
SELECT only, then runs client and Qt checks. It creates no application tables and
writes no password file. It stops if that database/account already exists.

For an already provisioned development database, use its development password:

```bash
bash scripts/dev-database.sh test --build-dir build-ucrt64
```

Add --client as above if necessary. Use inspect for a read-only administrator review.
The environment helper can optionally prompt/export the existing diagnostic password;
see Building.md. Root is never the account used for application connectivity tests.

## CentOS Stream 9 — packages and repositories

Use Bash in your terminal; VIM works throughout. Review installed packages first:

```bash
g++ --version
cmake --version
command -v ninja mariadb mysql
rpm -q qt6-qtbase-devel qt6-qtbase-mysql mariadb-connector-c-devel mariadb mariadb-server
```

For a new development machine, have an authorized administrator install the build
prerequisites:

```bash
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel \
   qt6-qtbase-mysql mariadb-connector-c-devel
```

For local database setup/testing also install the actual client and server:

```bash
sudo dnf install mariadb mariadb-server
```

Bash/coreutils are normally installed already. Optional editor/source tools:
`sudo dnf install vim-enhanced git`. A Qt 5 mysql plugin cannot serve a Qt 6 binary.
qt6-qtbase-devel includes development components needed for the default Qt tests;
qt6-qtbase-mysql supplies QMYSQL and connector runtime dependencies.

If Qt 6 packages cannot be found, review enabled repositories (`dnf repolist`).
CentOS Stream 9 packages may require **CRB and EPEL 9**. Enable them only after
administrator review, not by silently adding unrelated third-party repositories:

```bash
sudo dnf install dnf-plugins-core
sudo dnf config-manager --set-enabled crb
sudo dnf install epel-release
```

Retry the prerequisite install after repository setup. Consult the
[Fedora EPEL setup guide](https://docs.fedoraproject.org/en-US/epel/getting-started/)
for the current CentOS Stream 9/EPEL Next requirements if dependency resolution
still fails; do not substitute EL8/EL10 repositories. Background:
[Red Hat's CentOS Stream EPEL instructions](https://www.redhat.com/en/blog/install-epel-linux).
Review any existing MariaDB installation before selecting a different server vendor
repository or changing its version/data directory; this guide uses distribution
packages, not an automatic server upgrade or data-directory initialization.

## CentOS Stream 9 — server, build and database setup

Start the installed service and inspect its status/listener:

```bash
sudo systemctl start mariadb
systemctl is-active mariadb
sudo systemctl status mariadb --no-pager
ss -ltn 'sport = :3306'
```

Optional, if you want it to start after reboot: `sudo systemctl enable mariadb`.
Keep local TCP access available on 127.0.0.1:3306 for the helper. If a fresh server
needs explicit binding, add `bind-address=127.0.0.1` under its server configuration
section and restart with `sudo systemctl restart mariadb`; inspect existing clients
before altering a shared server. Do not open public SQL access for this validation.

For distribution installations with root Unix-socket authentication, `sudo mariadb
--no-defaults --protocol=SOCKET --user=root` opens the administrator client without
using an application's password. Do not reset root authentication merely to match
a Windows password workflow; the helper supports socket administration.

From the repository root, build and validate:

```bash
source scripts/dev-environment.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/TaskCompassDbDiagnostic
# Fresh dedicated database/account only:
sudo bash scripts/dev-database.sh setup --admin-socket --client /usr/bin/mariadb
# Or, for an existing development database, test using the restricted password:
bash scripts/dev-database.sh test --build-dir build
```

A local desktop session with Qt's X11/Wayland platform libraries is needed to run
`./build/TaskCompassPrototype` interactively. Headless CTest uses Qt's offscreen
plugin, so no Xvfb or full desktop installation is required just for automated
checks. A missing platform plugin is a runtime dependency issue; a missing
Qt6Config.cmake is a development-package/configuration issue.

## Installation, setup and test completion checklist

- Compiler/CMake/Ninja and matching Qt Widgets/SQL/Test packages are available.
- CMake configure/build and CTest pass; QMYSQL lists as available and loadable.
- MariaDB Server is installed, running and listening locally on port 3306.
- Client executable is found (or provided by --client); administrator setup uses
  the intended instance and stops for review if TaskCompass resources exist.
- The dedicated account authenticates over TCP, selects taskcompass_dev and sees
  an empty table list. Its exact-database SELECT-only grants are inspected without
  attempting CREATE/DROP TABLE.
- The Qt diagnostic connects, executes SELECT VERSION(), closes and exits 0.
- Passwords remain private, outside history/repository/output; no secret files are
  created by the repository helpers. Keep administrator and development passwords separate.

QMYSQL availability alone does not satisfy the last database checks. Windows build,
helper execution and actual GUI usability still require Windows-side evidence;
this guide does not mark them passed. Linux observations/pending checks stay in
[Roadmap.md](Roadmap.md). Production runtime/migration privileges and packaging are
future milestones, not prerequisites to create application tables during validation.
