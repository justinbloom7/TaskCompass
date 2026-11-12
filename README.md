<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# TaskCompass

A traditional desktop task manager organized around responsibilities, objectives,
projects, and guided planning at quarterly, sprint, weekly, and daily horizons.

**Status: Phase 0.1 accepted; Phase 1.1 source implemented, live validation pending.**
The original `TaskCompassPrototype` remains a disposable in-memory demonstration.
The new `TaskCompass` desktop and `TaskCompassMigrate` tool implement the persistent
foundation but have not yet been validated against a live migrated database.
Account provisioning and migration execution require owner approval. See the
[Phase 1.1 milestone report](docs/milestones/Phase1.1.md) for implementation,
test evidence, exact proposed SQL, recovery procedures and approval requests.
Milestone plans and results live under `docs/milestones/`.

[AGENTS.md](AGENTS.md) contains the permanent Codex development, safety and
milestone-reporting instructions for this repository.

Start with [Installation.md](docs/Installation.md) for Windows/UCRT64 and CentOS
Stream 9 prerequisites, MariaDB Server installation and service startup. See
[Building.md](docs/Building.md) for build/run/test commands, scripted database
setup, QMYSQL diagnostics and validation limitations.

Read in this order:

1. [Requirements and open decisions](docs/Requirements.md)
2. [Architecture proposal and technology comparison](docs/Architecture.md)
3. [Preliminary relational model](docs/DataModel.md)
4. [Phased implementation roadmap](docs/Roadmap.md)
5. [Desktop interactions: progress, contexts, ordering, dates and filters](docs/Interactions.md)
6. [Licensing and redistribution review](docs/Licensing.md)
7. [Repository inspection and checkbook reuse findings](docs/Inspection.md)

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

Production designs now distinguish Worked Today, Done for Today and Completed;
include undo/redo from the first persistence milestone; and specify numeric
priorities, user-defined context order and persistent drag/drop behavior. These
capabilities are **not implemented by the sample prototype**. Phase 1.1 adds only
independent task capture/edit/completion/reopen and transactional session undo/redo
in the separate persistent executable; later task/planning capabilities remain deferred. See the interaction
specification for confirmed requirements and recommended policies needing review.

TaskCompass project-owned code and documentation are free software: you may
redistribute and/or modify them under the GNU General Public License as published
by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version. SPDX: **GPL-3.0-or-later**. It is provided without
warranty; see [LICENSE](LICENSE). Third-party components retain their own licenses.
[Licensing.md](docs/Licensing.md) records dependency obligations and unresolved
native-iOS distribution concerns. No distribution exception is granted.
