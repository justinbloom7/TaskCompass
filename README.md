# TaskCompass

A traditional desktop task manager organized around responsibilities, objectives,
projects, and guided planning at quarterly, sprint, weekly, and daily horizons.

**Status: architecture review.** This repository contains planning documents only.
No application, database schema, dependencies, or synchronization service has been
implemented. C++20 with Qt 6 Widgets is the preferred desktop framework; schema
and remaining architecture proposals await review.

Read in this order:

1. [Requirements and open decisions](docs/Requirements.md)
2. [Architecture proposal and technology comparison](docs/Architecture.md)
3. [Preliminary relational model](docs/DataModel.md)
4. [Phased implementation roadmap](docs/Roadmap.md)
5. [Repository inspection and checkbook reuse findings](docs/Inspection.md)

Development targets CentOS Stream 9 with a terminal/VIM workflow. Offline desktop
use comes first, with MariaDB on Linux and Windows builds on Windows. A native
iPhone app (possibly Android later) will have a separate UI, potentially fewer
features, and must also work offline. Qt Quick/QML is a provisional mobile option.
Hosted storage must remain possible through a future API. The phone will synchronize its offline
database bidirectionally with a TaskCompass service running on Linux or Windows
over the internet. Shared C++ domain/application logic stays independent of
Widgets and QML. See the architecture proposal for layer boundaries, coding
preferences, and remaining implementation decisions.
