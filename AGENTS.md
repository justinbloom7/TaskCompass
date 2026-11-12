<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# TaskCompass repository instructions

These are permanent project-wide development and reporting instructions. Read
them before working in this repository. Keep milestone-specific scope, decisions,
approvals and results in the applicable milestone document, not in this file.
Explicit owner instructions take precedence; identify design conflicts rather
than silently introducing architectural or scope changes.

## Design baseline and conventions

Review the relevant authoritative design documents before changes:

- [Requirements](docs/Requirements.md).
- [Architecture](docs/Architecture.md).
- [Data model](docs/DataModel.md).
- [Interactions](docs/Interactions.md).
- [Roadmap](docs/Roadmap.md).

Use [Building](docs/Building.md), [Installation](docs/Installation.md) and the
applicable milestone for commands and actual validation evidence. Distinguish
confirmed requirements, proposed designs, implemented behavior and historical
observations. Later explicit decisions supersede earlier proposals; preserve the
record of that transition.

- Desktop: C++20, Qt 6 Widgets, CMake and MariaDB. Preserve Linux and Windows
  support. Development must work from a terminal with VIM; do not require an IDE
  or introduce editor-specific workflows. Windows builds run on Windows or a VM;
  Linux-to-Windows cross-compilation is not required.
- Keep domain and application logic independent of GUI and persistence
  implementations. Put Widgets presentation and database-specific concerns in
  separate adapters. Follow the approved transaction, revision and command design.
- Constants: `UPPER_CASE_SNAKE_CASE`. Variables/functions: `camelCase`.
  Classes and corresponding filenames: `PascalCase`.
- Use 3-space indentation and opening braces on the declaration/control line.
  For multiline declarations/calls, put the first parameter immediately after
  `(`, align subsequent parameters vertically, and keep `)` on the final parameter
  line. Use required Qt interface/override names; discuss other language-specific
  naming exceptions before introducing them.
- Favor focused, maintainable code. Avoid unnecessary dependencies, speculative
  abstractions, unrelated refactoring and general GUI redesign. Do not install
  dependencies or modify system packages without approval.
- Implement only the authorized milestone scope. Future requirements influence
  boundaries but do not authorize implementing future features. Preserve existing
  functionality and the accepted prototype unless a change is explicitly in scope.
- Preserve GPL-3.0-or-later and documented third-party licensing obligations.

## Milestone record

Every implementation milestone must have a Markdown document at
`docs/milestones/Phase<version>.md`, such as
[Phase 1.1](docs/milestones/Phase1.1.md). Create or review it before implementation.
It is the authoritative record of that milestone's implementation and review
history; the roadmap remains the cross-milestone overview.

Each milestone document must include:

1. Status and scope.
2. Prerequisites and relevant baseline information.
3. Implementation plan and architectural decisions.
4. Files created or modified.
5. Database schema and migration changes, when applicable.
6. Tests performed, exact commands and actual results.
7. Known issues, limitations and deferred work.
8. Approval requests and decisions.
9. Remaining work and completion criteria.

Use dated progress entries and clear current-status pointers. Preserve
chronological progress, prior decisions, failures and results. Append or label
superseding information; do not silently replace historical evidence with newer
claims. Mark nonapplicable items explicitly rather than implying work was done.

## Reporting and review

Before concluding a significant implementation step or requesting approval:

1. Update the milestone document with the complete technical report needed for
   independent review, including changes, decisions, files and validation.
2. Clearly distinguish implemented, proposed, tested, untested and pending items.
   Separate agent-executed evidence from owner-reported verification and assumptions.
3. Include exact commands, concrete SQL and relevant error messages where
   appropriate, with credentials and sensitive values omitted or redacted.
4. Identify unresolved decisions and outstanding work. When authorization is
   needed, include a clearly labeled **Approval Requested** section describing
   the exact action, target, scope, risks and applicable recovery procedure.
5. Finish updating the document before stopping at an approval checkpoint.

Keep terminal responses brief: summarize the outcome, files changed and conflicts,
and point to the milestone for the full report. Do not leave essential review
information only in the conversation. The owner commits and pushes files for
external review; that intention is not authorization for Codex to do so.

## Approval checkpoints

Stop and request explicit approval before:

- Executing live database migrations.
- Creating/changing database accounts or privileges.
- Destructive or potentially irreversible operations.
- Major architectural changes outside the approved plan.
- Expanding an approved milestone's scope.
- Committing, merging, tagging or pushing repository changes.

Source-code approval does not imply live database approval. Never infer approval
from an earlier milestone or from silence. Honor explicit authorization already
given for the specific action and scope; do not broaden it. Prepare the concrete
SQL/procedure and complete safe, authorized source/documentation work before
requesting the final approval needed for execution.

## Database safety

- Access only explicitly authorized TaskCompass databases. Never access or modify
  unrelated databases, including eEnvelopes; do not reuse their credentials.
- Preserve separate diagnostic, runtime, migration and test privileges. Use
  least-privilege accounts, bound query parameters and approved transaction rules.
  Do not use elevated accounts for application connectivity.
- Never commit credentials or put them in source, build artifacts, logs or command
  history. Use the documented secret-handling mechanism; examples use placeholders.
- Integration tests require an explicitly configured, isolated database. Ordinary
  tests must not implicitly connect to a live database. Never destructively reset
  a development database as test cleanup.
- Do not automatically repair, reset or destroy database contents. Any deliberate
  destructive test/reset requires explicit approval for that isolated target and
  procedure; keep it opt-in and fail closed on unexpected identities or contents.
- Document migration ordering/version checks, partial-failure detection, recovery
  and backup/rollback limitations before requesting execution approval. Do not
  assume MariaDB DDL rolls back transactionally or silently repair schema history.

## Validation and completion

- Run relevant builds and automated tests after changes. For documentation-only
  work, validate Markdown references and consistency; explain why application
  build/tests were not rerun when they are unaffected.
- Preserve Linux/Windows compatibility. Distinguish tests actually executed from
  tests pending owner execution or unavailable platforms/services.
- Do not claim validation success without evidence. Record failures, relevant
  errors, unresolved issues and unexecuted checks in the milestone document.
- A successful compile is not milestone completion. Check the approved acceptance
  criteria, required runtime/manual/platform checks and remaining work before
  marking a milestone complete.
