<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.7 Linux operator checklist — NOT EXECUTED

CentOS Stream9 / Qt6 Widgets / MariaDB10.5.29, server feledria,
`taskcompass_dev@127.0.0.1:3306`, dataset
`21be9da7-b767-4578-b972-cc9a35122ddb`. Runtime user taskcompass_app only.
[Full procedure and expected results](Phase1.2B77.md) are authoritative for these
checkboxes. Manual PASS requires owner observation. No live testing authorized by
preparation alone; Linux approval must cover only new prefix-qualified QA records.

## Commands

```bash
scripts/phase12/b77/01-build.sh
```

After Linux B7.7 approval, with all TaskCompass clients closed:

```bash
scripts/phase12/b77/02-preflight.sh --approved --platform linux
qaDirectory='/actual/private/b77-directory/printed/by/preflight'
scripts/phase12/b77/03-launch.sh --approved --platform linux --qa-dir "$qaDirectory"
```

Use the printed QA prefix and copy titles from `$qaDirectory/qa-names.tsv`. Fill
G01–G07, then close normally before checkpoint. Passwords: taskcompass_migrate and
taskcompass_dev for native evidence, taskcompass_app for runtime, sudo for root
metadata. No root database password/plugin change. No typed confirmations.

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform linux \
   --qa-dir "$qaDirectory" --label before-restart
scripts/phase12/b77/03-launch.sh --approved --platform linux --qa-dir "$qaDirectory"
```

G08 is read-only: observe persistence/empty session undo/filter preferences, close:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform linux \
   --qa-dir "$qaDirectory" --label after-restart --compare before-restart
```

For G09, launch03 in two terminals using the same qaDirectory. Close both afterward:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform linux \
   --qa-dir "$qaDirectory" --label final
```

Private UUID/owner/archive/status/revision inventories are in each checkpoint's
inventory.tsv. Capture observations in a private copy of this checklist, not in a
credential file or a public screenshot of original tasks. Do not rerun02 after QA
writes; retests use03/04 with fresh labels and preserved records.

## Owner result record

Version/source details are generated automatically by build/QA scripts; see
[BuildNotes](BuildNotes.md). Record manual observations and QA identifiers below.

Record manual observation time, run path/prefix, inventory paths and each QA
role's UUID; version/source details are already in the generated reports. Leave failures/unexecuted
checks explicit; do not turn helper PASS into manual PASS.

- [ ] Build/CTest succeeded; two live integration suites skipped (not live passes).
- [ ] Initial preflight/seals/pristine original dataset/final grants passed.
- [ ] G01 original active/completed views unchanged; tombstones retained by export.
- [ ] G02 QA pillar/subpillar/project/subproject/objective create/edit/rename and
  deep navigation; duplicate objective names distinguishable by UUID/type/context.
- [ ] G03 selected-owner defaults/Independent choice, illegal/missing/archived owner
  rejection, cancellation and title/notes validation; original tasks never saved.
- [ ] G04 moves/subtree UUID stability and all shared Undo/Redo operations.
- [ ] G05 archive inheritance, explicit flags, Locate Archived Ancestor/restore.
- [ ] G06 QA completion/reopen, timestamps and independent archive/completed filters.
- [ ] G07 closed cleanly and before-restart checkpoint passed.
- [ ] G08 restart values/view selection/expansion persisted, Show Completed and Show Archived off,
  Undo/Redo empty; after-restart full-nine-table equality passed.
- [ ] G09 two-session stale edit/inverse, dependent undo-create and stale/missing
  parent rejection; no overwrite/partial persistence; final checkpoint passed.
- [ ] QA inventory UUIDs/owners/revisions checked against manual observations.
- [ ] Conflicted-history/Reload policy reviewed; no automatic merge/reset assumed.
- [ ] Permission-denial/real connection interruption marked NOT EXECUTED unless
  separately safely scoped; offline coverage is recorded separately.
- [ ] All unexpected results filed as defects/review items; no automatic cleanup.
- [ ] Owner reports results for Linux review before Windows authorization.

No service shutdown, SQL KILL, grants, migrations, reset/restore or original-record
mutation. Do not delete QA rows or artifacts. GUI completion does not imply B7.8/
B7.9 backup/restore or final Phase1.2 acceptance.
