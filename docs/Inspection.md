# Initial inspection

Inspection date: 2026-11-09. This records observed implementation separately from
TaskCompass proposals. No checkbook files were modified or executed.

## TaskCompass

The initial working tree was clean and contained only `README.md` (a title and
“Task Manager”), Git metadata, and environment-managed `.agents`, `.codex`, and
`.aws` directories. No application source, build system, dependency manifest,
schema, tests, or project-specific AGENTS.md was found. No ancestor AGENTS.md was
found at `/home/justin` or `/home/justin/repo`. Environment-managed directories
were left untouched.

The host reports CentOS Stream 9, Python 3.9.25, and glibc 2.34. These observations
do not establish that a particular GUI binary distribution will work. No packages
were installed or GUI/toolchain compatibility tests performed.

## Checkbook examined read-only

The available active project is `../ckbk-envelopes`, branded **eEnvelopes**.
`../ckbk-envelopes.bak` also exists; it was not used as the architectural reference.
Read the active project's README.md, PROJECT_NOTES.md, and AGENTS.md, and inspected:

| Source relative to checkbook root | Evidence and relevance |
| --- | --- |
| `src/lib/db/Database.ts` | Abstract persistence API with domain operations and an identity store. |
| `src/lib/db/MariaDatabase.ts` | UUID identifiers, transactions for related writes, retry payload checks, and initialization-time schema adjustments. |
| `src/lib/db/schema.sql` | MariaDB tables and relational constraints; no TaskCompass schema is implied. |
| `src/lib/offline.ts` | IndexedDB per-user snapshots and pending entries; persist-before-upload, stable retry IDs, serialized uploads, retained errors. |
| `src/app/api/transactions/route.ts` | Authentication and account authorization before creating/retrying entries. |
| `src/lib/auth/access.ts` | User-scoped retry identity and payload hashing; server-side permission checks. |
| `tests/offline.test.ts` | Tests for lost responses, stable retry IDs, user isolation, rejected entries, and retained queues. Tests were read, not run. |

The checkbook uses TypeScript/Next.js with MariaDB/MySQL as the normal server
store and a disposable MemoryDatabase alternative. Its current mobile companion
is an installable web app. Offline support covers cached views and queued
payments/deposits, including splits; transfers and setup require connectivity.
This is **limited offline entry**, not a general multi-device database replication
engine. Its documentation does not establish general edit/delete conflict handling
or a cursor-based incremental download protocol.

## Reuse recommendation

Reuse patterns first, not financial entities or the entire framework:

- Keep domain calculations/validation outside UI and storage adapters.
- Use narrow persistence interfaces and transactions for multi-record operations.
- Persist pending operations before networking; retain rejected work for review.
- Give retries stable operation identities and reject changed payloads under the
  same identity. Recheck authorization on the server.
- Carry forward failure scenarios from offline tests as protocol conformance cases.

TaskCompass should improve on initialization-time schema adjustments with explicit,
numbered migrations and backup/restore verification. A database implementation
switch alone does not provide offline synchronization.

Direct TypeScript/IndexedDB code reuse is limited with the C++20/Qt client.
Extracting a shared package now would couple two changing projects.
Later, agree on a language-neutral synchronization envelope and conformance tests,
then consider a small transport/retry library where language overlap exists.
Keep independent databases, migrations, domain validators, API namespaces, and
credentials/scopes. Financial access rules and ledger semantics stay in eEnvelopes.
Do not retrofit the checkbook as part of this project.

## Subsequent platform/storage clarification

The user confirmed MariaDB on Linux, builds on a Windows host/VM, and a native
iPhone app, with Android possible later. The architecture now follows the
checkbook's pluggable database pattern more directly, while proposing MariaDB for
standalone desktop and hosted storage, and SQLite for embedded mobile storage.
The subsequent UI decision selects C++20/Qt 6 Widgets as the preferred traditional
desktop framework. Mobile will have a separate, potentially reduced presentation;
Qt Quick/QML is only a provisional mobile option. Shared C++ domain/application
logic stays independent of both presentations. The next proposed milestone is a
small Widgets/CMake/MariaDB-driver prototype on CentOS and Windows, requiring
explicit instruction before work starts. Mobile validation is deferred.

All six TaskCompass documents were reviewed for this UI revision. No UI dependency
or inconsistency requiring a relational-model change was found; DataModel.md is
unchanged. Storage and synchronization design remain unchanged. Checkbook inspection
findings above are unchanged; this revision did not access or modify that project.

## Phase 0.1 environment inspection and prototype

On 2026-11-10, GCC 11.5.0, CMake 3.31.8, Ninja 1.10.2, MariaDB 10.5.29 packages and
Connector/C 3.2.6 runtime were present. Qt 6 development/QMYSQL and connector headers
were initially missing; the Qt 5 mysql plugin was not usable by the new Qt 6 build.
After an authentication-blocked sudo attempt, the user confirmed installation of
Qt 6.10.3 development/QMYSQL packages and Connector/C 3.2.6 headers. The prototype
now compiles; focused tests and offscreen/actual-display startup pass. Live isolated
MariaDB connectivity and Windows validation remain pending. See
[Building.md](Building.md) and [Roadmap.md](Roadmap.md) for reproducible results.
This iteration did not connect to, run, inspect configuration from or modify
eEnvelopes. The prototype's Qt fixture types make no decision about the future
shared C++ domain's use of standard-library versus Qt Core value types.

## Phase 0.2 review — documentation and license only

Reviewed all seven requested documents, CMake and the prototype navigation/model/
filter/dialog/action/test code. The sample model stores presentation strings and
fixture tags, the status editor directly changes sample rows, and sorting is a
proxy operation. Those remain prototype behavior, not production lifecycle,
organizational, numeric-priority, context, work-recording or undo implementations.
No source, test, CMake, database, build dependencies or eEnvelopes files were changed.

Added Interactions.md and Licensing.md; refined requirements, command architecture,
logical persistence candidates and phased acceptance gates. LICENSE is an exact
copy of the standard GPLv3 text shipped in the local Qt package; the README grant
and SPDX identifier select GPL-3.0-or-later for TaskCompass. Reviewed installed RPM
license metadata and official Qt/SPDX references. Desktop adoption has no identified
blocker; artifact-specific notices/source obligations and native-iOS distribution
review remain. No Phase 0.1 pending check is marked passed by this design work.
