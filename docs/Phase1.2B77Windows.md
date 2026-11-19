<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.7 Windows operator checklist — NOT EXECUTED / NOT AUTHORIZED

Windows / MSYS2 **UCRT64** / Qt6 Widgets / MariaDB13.0.2, server feldria-w10,
`taskcompass_dev@127.0.0.1:3306`, dataset
`457bb52b-fea4-4347-9c47-675c6b035c90`. Runtime user taskcompass_app only.
Wait for Linux results and independent Windows B7.7 approval. Linux success does
not validate Windows. Follow [full expected results](Phase1.2B77.md).

## Commands (after separate Windows authorization)

Update/build in UCRT64 using Windows-installed Qt/compiler, no cross compilation:

```bash
scripts/phase12/b77/01-build.sh
scripts/phase12/b77/02-preflight.sh --approved --platform windows
qaDirectory='/actual/private/Windows/b77-directory/printed/by/preflight'
scripts/phase12/b77/03-launch.sh --approved --platform windows --qa-dir "$qaDirectory"
```

The wrapper uses build-ucrt64, TaskCompass.exe and the known Windows native
MariaDB client. Passwords are the **Windows** account passwords. Administrator
uses native Windows root password, never sudo. MSYS2 password path conversion is
excluded; no credential is stored. No typed confirmation phrases.

Complete G01–G07, close normally, then:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label before-restart
scripts/phase12/b77/03-launch.sh --approved --platform windows --qa-dir "$qaDirectory"
```

G08 makes no mutations; close afterward:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label after-restart --compare before-restart
```

For G09 use two UCRT64 terminals running03 with this same Windows qaDirectory.
Close both, then:

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label final
```

Use Windows run prefix/titles/UUID inventory; never paste Linux paths/passwords.
Do not use Linux-only07 recovery or repeat old migration/runtime grant stages.
Do not rerun02 after QA writes; resume03/04 with fresh checkpoint labels.

## Owner result record

Version/source details are generated automatically by build/QA scripts; see
[BuildNotes](BuildNotes.md). Record manual observations and QA identifiers below.

Use generated version/source reports; record manual observation time, run
path/prefix and each QA UUID/inventory path. All boxes
remain unchecked until owner executes and observes each test.

- [ ] Windows build/CTest passed; live suites skipped rather than falsely passed.
- [ ] Preflight private ACLs/seals/schema2/UUID/original data/final grants passed.
- [ ] G01 original active/completed tasks unchanged; tombstones preserved by export.
- [ ] G02 create/edit/rename nested hierarchy/flat objectives; duplicate names,
  splitters, resizable columns, keyboard/dialog/table/navigation behavior usable.
- [ ] G03 all legal task ownership transitions, independent capture, illegal owner,
  missing/archived parent/cancel and validation cases correctly reject.
- [ ] G04 subtree moves and shared chronological Undo/Redo; stable UUIDs/descendants.
- [ ] G05 explicit/inherited archive, ancestor location and restore distinctions.
- [ ] G06 completed/reopened timestamps and Show Completed/Show Archived combinations.
- [ ] G07 before-restart capture passed; states/UUIDs recorded privately.
- [ ] G08 runtime reconnect after normal restart, persisted relationships/views/
  expansion/archive/completion; empty undo and exact nine-table equality passed.
- [ ] G09 two-session conflicts/missing parent/dependent inverse refuse atomically;
  stale changes never overwrite another session; final checkpoint passed.
- [ ] Current QA UUID/ownership/archive inventory matches GUI observations.
- [ ] Actual conflict-history/Reload behavior reviewed without assuming stack reset.
- [ ] Real interruption/permission-denial GUI cases marked NOT EXECUTED unless
  separately authorized; offline/isolated coverage reported distinctly.
- [ ] Defects and untested items reported, no QA purge or original-data edit.
- [ ] Owner results reviewed before subsequent backup/restore/final acceptance gates.

No grants/password/plugin changes, service stop, SQL KILL, migrations, reset or
restore. Preserve both platform backups/rehearsals, all QA records and artifacts.
