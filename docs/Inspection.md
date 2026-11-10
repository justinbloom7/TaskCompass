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

Direct TypeScript/IndexedDB code reuse is limited with a native C++/Qt client or the Python desktop alternative. Extracting a shared package now would couple two changing projects.
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
C++/Qt Quick is the revised leading cross-platform client proposal; Python is
retained as an alternative, not an approved implementation choice. Checkbook
inspection findings above are unchanged; the checkbook has not been modified.
