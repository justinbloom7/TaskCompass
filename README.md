# TaskCompass

A traditional desktop task manager organized around responsibilities, objectives,
projects, and guided planning at quarterly, sprint, weekly, and daily horizons.

**Status: Phase 0.1 technical prototype, partially validated.** The C++20/Qt 6
Widgets desktop runs with disposable in-memory samples and an optional, separate
MariaDB connectivity diagnostic. No TaskCompass domain system, task persistence,
schema, migrations or synchronization service has been implemented. Windows and
live database validation remain pending; architecture/schema review continues.

See [Building.md](docs/Building.md) for prerequisites, build/run/test commands,
optional QMYSQL diagnostics, Windows instructions and validation limitations.

Read in this order:

1. [Requirements and open decisions](docs/Requirements.md)
2. [Architecture proposal and technology comparison](docs/Architecture.md)
3. [Preliminary relational model](docs/DataModel.md)
4. [Phased implementation roadmap](docs/Roadmap.md)
5. [Repository inspection and checkbook reuse findings](docs/Inspection.md)

Organization uses nested pillars and projects, with flat objectives owned by one
pillar or project. Tasks belong to one objective, directly to one project, or remain
independent. Objective tasks derive their context from the objective. Sprint
selection is independent of ownership; project-, objective- and sprint-oriented
views reference the same records. See the [logical model](docs/DataModel.md).

Development targets CentOS Stream 9 with a terminal/VIM workflow. Offline desktop
use comes first, with MariaDB on Linux and Windows builds on Windows. A native
iPhone app (possibly Android later) will have a separate UI, potentially fewer
features, and must also work offline. Qt Quick/QML is a provisional mobile option.
Hosted storage must remain possible through a future API. The phone will synchronize its offline
database bidirectionally with a TaskCompass service running on Linux or Windows
over the internet. Shared C++ domain/application logic stays independent of
Widgets and QML. See the architecture proposal for layer boundaries, coding
preferences, and remaining implementation decisions.
