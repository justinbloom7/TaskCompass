# TaskCompass

A personal task manager organized around responsibilities, objectives, projects,
and guided planning at quarterly, sprint, weekly, and daily horizons.

**Status: architecture review.** This repository contains planning documents only.
No application, database schema, dependencies, or synchronization service has been
implemented. Technology and schema recommendations await review.

Read in this order:

1. [Requirements and open decisions](docs/Requirements.md)
2. [Architecture proposal and technology comparison](docs/Architecture.md)
3. [Preliminary relational model](docs/DataModel.md)
4. [Phased implementation roadmap](docs/Roadmap.md)
5. [Repository inspection and checkbook reuse findings](docs/Inspection.md)

Development targets CentOS Stream 9 with a terminal/VIM workflow. Offline desktop
use comes first, with MariaDB on Linux and Windows builds on Windows. A native
iPhone app (possibly Android later) must also work offline. Hosted storage must
remain possible through a future API. See the architecture
proposal for coding preferences and language convention conflicts to resolve before
implementation.
