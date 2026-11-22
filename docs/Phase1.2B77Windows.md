<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.7 Windows operator checklist — NOT EXECUTED / NOT AUTHORIZED

Windows / MSYS2 **UCRT64** / Qt6 Widgets / MariaDB13.0.2, server feldria-w10,
`taskcompass_dev@127.0.0.1:3306`, dataset
`457bb52b-fea4-4347-9c47-675c6b035c90`. Runtime user taskcompass_app only.
Wait for Linux results and independent Windows B7.7 approval. Linux success does
not validate Windows. Follow the numbered walkthrough below; [full expected results](Phase1.2B77.md)
provide background. This is prepared documentation, not a claim of execution.

## Numbered walkthrough — use in order after Windows authorization

**Do not use your own existing tasks for this run.** Every created name must be
copied from the generated Windows QA name list. Existing records are for inspection
only: never save an editor, toggle completion or change ownership on them. There
is no automatic QA-only lock in the application. The preservation check will stop
if any original row changes, even if its visible values are later undone.

Keep this page and one UCRT64 terminal open throughout. Mark each numbered section
complete only after its expected result. **At each CHECKPOINT, close every GUI,
run the command, and wait for PASS before continuing.** Do not combine those commands
with the next launch. If any step fails, stop and report its output; do not repeat
preflight, delete evidence, rerun a failed label or repair records.

### 1. Build, establish the private QA run and copy its names

After separate Windows B7.7 authorization, with all TaskCompass windows closed:

```bash
scripts/phase12/b77/01-build.sh
scripts/phase12/b77/02-preflight.sh --approved --platform windows
```

Expected: build/offline tests pass (two live suites skipped); preflight PASS prints
one Windows QA directory. At the following prompt paste that **complete** printed
b77 directory, not its parent. This is actual missing path information, not an
approval phrase. Never paste the Linux QA directory.

```bash
IFS= read -r -p 'Paste the full Windows QA directory printed above: ' qaDirectory
qaPrefix=$(cat "$qaDirectory/prefix.txt")
cat "$qaDirectory/qa-names.tsv"
```

The list contains full titles for Root A, Root B, Sub 1, Sub 2, Project, Subproject,
Project outcome, Pillar outcome, Subpillar outcome, Independent task, Direct task,
Objective task, Duplicate, History root, Missing parent and Stale task.
**Below, “Root A” means the full generated title ending in “ Root A”, including
its B77-QA-windows-… prefix.** Copy the full title every time; do not type just
Root A or substitute your own names. Keep the printed list handy. Retain the
variables in this terminal; if it closes, re-enter the same actual QA directory
and read prefix.txt again, without rerunning preflight.

Passwords are the Windows MariaDB account passwords. The native administrator
prompt is for Windows MariaDB root, never a Linux/sudo password. Helpers retain
credentials only in process memory; no typed approval strings.

```bash
scripts/phase12/b77/03-launch.sh --approved --platform windows --qa-dir "$qaDirectory"
```

### 2. Inspect originals, then create exactly this hierarchy (G01/G02)

1. In All Tasks, clear search and inspect the existing active tasks. Enable Show
   Completed to inspect existing completed tasks, then turn it off. Do not change
   any original checkbox or save an original task editor. Original tombstones
   stay hidden; helpers check their preservation later.
2. Press Ctrl+F and paste the full qaPrefix printed by step1 into search. The task
   list is initially empty. Keep this filter for all task mutations below. The
   organization tree is not filtered by task search: check the full QA name before
   every organization edit. If in doubt, cancel the dialog.
3. Click All Tasks. Click **+ → New Pillar**, paste the full **Root A** title and
   save. Repeat **+ → New Pillar** for **Root B**. These are root pillars.
4. Click QA **Root A** in the tree. **+ → New Subpillar**; paste **Sub 1**, save.
5. Click QA **Sub 1**. **+ → New Subpillar**; paste **Sub 2**, save.
6. Click QA **Sub 2**. **+ → New Project**; paste **Project**, save.
7. Click QA **Project**. **+ → New Subproject**; paste **Subproject**, save.
8. Click QA **Subproject**. **+ → New Objective**; paste **Project outcome**, save.
9. Click QA **Root A**. **+ → New Objective**; paste **Pillar outcome**, save.
10. Click QA **Sub 1**. **+ → New Objective**; paste **Subpillar outcome**, save.
11. Click QA **Project outcome**. **+ → New Task**; paste **Objective task**. Before
    saving check Owner ends in Project outcome. Save.
12. Click QA **Project**. **+ → New Task**; paste **Direct task**. Before saving
    check Owner ends in Project (not Subproject or Project outcome). Save.
13. Click **Inbox — independent tasks**. **+ → New Task**; paste **Independent
    task**. Owner must read Independent. Save.

Expected tree (all names include the full QA prefix; icons supply entity types):

```text
Root A (2)
  Sub 1 (2)
    Sub 2 (2)
      Project (2)
        Subproject (1)
          Project outcome (1)  [Objective task belongs here]
        [Direct task belongs directly to Project, shown in task list]
    Subpillar outcome (0)
  Pillar outcome (0)
Root B (0)
[Independent task is in Inbox, not in this hierarchy]
```

Sibling display is alphabetical within type groups, so the diagram describes
ownership rather than literal screen row order. Verify Pillars separator, compact
single-line task rows, icons, counts and full type/path/UUID tooltips. Expand to
Project outcome; switch objective-oriented navigation and back. No relationships
change. Search for a nonmatching string: counts stay unchanged. Restore qaPrefix.

**CHECKPOINT A — stop here, close the GUI normally, then run:**

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label hierarchy
```

Expected: protected rows unchanged, QA invariants/health/final grants PASS and
checkpoint sealed. If not PASS, do not proceed. Then relaunch03 using step1's
launch command and keep the QA search filter.

### 3. Editors, ownership, validation and creation menus (G02/G03)

1. Right-click QA Project and compare creation entries with +: New Task first;
   Subproject/Objective legal children; New Pillar last in creation section. Under
   a pillar expect Subpillar/Project/Objective; under an objective no child
   organization actions. There are no standalone New Task/New Pillar toolbar buttons.
2. Right-click QA Project → Edit / Rename. Append ` renamed` to its full generated
   title and enter description `Windows QA description`. Save; Undo restores the
   original title/description, Redo restores the edit, Undo again leaves the original
   **Project** title. Later instructions therefore still use Project.
3. Edit QA Independent task → Choose Owner: select QA Project, save (Root A3,
   Project3). Edit the same task → choose QA Project outcome, save (Subproject2,
   Project outcome2). Edit again → choose Independent, save. Counts return to step2.
   Check task UUID stays the same in checkpoint inventories.
4. Click QA Project outcome → + New Task: Owner defaults there; cancel. Under QA
   Project it defaults to Project; cancel. Under QA Root A it defaults Independent;
   cancel. In Choose Owner, pillars cannot be selected as task owners.
5. Under QA Root A create Objective with full **Duplicate** title. Under QA Root B
   create another Objective with the **same full Duplicate** title. Expect distinct
   records with type/path/UUID disambiguation, each count0. Do not rename originals.
6. Open New Task from Inbox: empty/only-spaces title disables OK. Enter the full
   generated Stale task title, then cancel: no record is created in this step.
   Optional oversized UTF-8 title/notes rejection is not implied passed; record
   NOT EXECUTED if omitted. Never use an original task for invalid-input tests.
7. Use QA Sub 1 → Move to inspect destinations. Self/descendants, projects as a
   pillar parent and objectives as any parent are unavailable; cancel. A project/
   objective cannot have No parent. Do not bypass UI validation with SQL.

### 4. Drag-and-drop and shared history (G04)

1. In project-oriented navigation drag **QA Project onto QA Root B**, not between
   rows. The existing Move picker opens with Root B selected; cancel first. Project
   must remain under Sub 2 with counts unchanged.
2. Repeat the same drop. Accept the selected Root B destination, inspect the
   subtree/task impact preview, then complete the move. Expected: Project and all
   its descendants appear under Root B; Root A0, Root B2, Project2. Tasks keep their
   owner UUIDs. Expand Project to inspect Subproject/Project outcome.
3. Undo once: Project returns under Sub 2, Root A2/Root B0. Redo once: it returns
   under Root B, Root A0/Root B2. **Leave it under Root B.** Check selection/expansion
   survives refresh and alphabetical order does not depend on counts.
4. Try dragging QA Root B onto its descendant QA Subproject, QA Sub 1 onto QA Project,
   and QA Project onto an objective: each must be rejected with no mutation. No
   task or between-row manual-order dragging is part of Phase1.2.
5. Move QA Pillar outcome through Move to QA Project, Undo, Redo, Undo; leave it
   under Root A. Move QA Sub 1 to Root B, Undo; leave it under Root A.
6. On QA Project edit description to `mixed history`; on QA Direct task edit notes
   to `mixed history`; archive QA Project. Undo three times, observing archive,
   task notes, organization description reverse in that order. Redo three times,
   then Undo once so Project is active. Both edited notes/descriptions may remain.
   Edit QA Project description to `Windows final description` and save: this new
   edit after Undo must disable Redo. Keep the original full Project title.

**CHECKPOINT B — close the GUI normally, then run:**

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label ownership-moves
```

Wait for PASS before relaunch03. Do not skip this checkpoint.

### 5. Explicit/inherited archive and completion (G05/G06)

1. Archive QA Subproject, then QA Root B (right-click → Archive; inspect previews).
   Turn Show Archived on. Both show explicit archive; Project inherits Root B's
   archive. Their open-task counts are0 even with Show Archived enabled.
2. Restore QA Subproject while Root B stays archived. Its explicit flag clears,
   but its task remains hidden without Show Archived. Locate Archived Ancestor
   takes you to Root B. It does not restore anything by itself.
3. Archive QA Subproject again. Restore QA Root B: Project/Direct task return
   active, but Subproject stays explicitly archived (Root B1, Project1).
4. Restore Subproject separately (Root B2, Project2). Undo/Redo restore and inspect
   its flag/count. Creation menus under an archived entity offer no child creation.
   New Task there defaults Independent. Cancel any observation-only editor.
5. Archive QA Root A, then use Move to place QA Project under archived QA Sub 2.
   The preview must warn about the archived destination. Complete, inspect inherited
   archive, then Undo the move and Undo Root A's archive. Leave Project under active
   Root B and Root A active. No original organization is involved.
6. Select QA Project in the tree; select **QA Direct task** in the filtered task
   list. Complete it: default list hides it, count falls from2 to1. Show Completed
   reveals it and a completion time. Reopen it (count2); Undo (count1); Redo
   (count2). Complete it again and **leave Direct task completed**.
7. Archive QA Subproject again and **leave it archived**. Root B/Project now count0;
   Objective task is active but effectively archived. Independent task stays active
   in Inbox. Toggle Show Completed/Archived separately to verify these distinctions.

### 6. Mandatory before-restart checkpoint (G07) — CLOSE GUI NOW

Enable Show Archived and select archived QA Subproject. Record its UUID privately
from the tooltip; note expanded paths. Close the GUI normally and wait for exit.

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label before-restart
```

**Wait for PASS. Do not launch again without it.** Inventory is in
`$qaDirectory/before-restart/inventory.tsv`. Helpers record version evidence.

### 7. Restart observation — NO WRITES during this entire section (G08)

```bash
scripts/phase12/b77/03-launch.sh --approved --platform windows --qa-dir "$qaDirectory"
```

Expected: Show Archived and Show Completed off; archived Subproject hidden.
Its saved hidden selection may fall back to All Tasks. Undo/Redo empty. Geometry,
visible navigation preferences and expansion restore where applicable. Enable
Show Archived/Completed for **observation only**: Subproject remains archived,
Direct task remains completed, Objective task keeps its owner. No create, edit,
move, completion, archive or Undo/Redo operation in this section. Close normally.

**CHECKPOINT C — immediately after closing, before any further GUI writes:**

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label after-restart --compare before-restart
```

Expected: exact nine-table equality and preservation PASS. UI preference toggles
are allowed; database mutations are not. This comparison cannot be reconstructed
later if missed. Stop if it fails.

### 8. Two-session conflict cases (G09) — exact sequences, QA only

Open two UCRT64 terminals A/B, both with the same actual Windows qaDirectory.
Use the step7 launch command in each; in both GUIs keep search set to qaPrefix.
Use F5 to reload when instructed. Wait for Working… to clear after every save.

1. **Stale edit:** A selects Inbox, creates the full **Stale task** title,
   Independent. B reloads. A opens Stale task editor and enters notes `session A`
   without saving. B edits that same QA task notes to `session B` and saves. A then
   saves: expect rejection. A reloads and sees session B. Never substitute an
   original task. Both editors must refer to that same full-prefixed QA title;
   the final private inventory records its UUID (task tooltips show notes, not UUIDs).
2. **Stale inverse:** after reload A edits Stale task notes to `A committed`, saves.
   B reloads, edits it to `B committed`, saves. A Undo rejects without overwriting
   B. Reload A; note this does not necessarily erase the failing undo entry.
3. **Missing parent:** B clicks QA Root A, creates the full **Missing parent**
   title as a Project. A reloads, selects that QA Project, opens + New Task,
   enters full qaPrefix plus ` Missing-parent child`, but does not save. B Undo
   immediately undoes creation of the empty Missing parent project. A saves:
   expect stale/missing-parent rejection and no child task. Keep soft-removal/history.
4. **Dependent undo-create:** A reloads after the rejected save, then clicks All Tasks and creates full **History root**
   title as a Pillar. B reloads and adds a subpillar named full qaPrefix plus
   ` History child` under it. A Undo rejects removal of the nonempty root. Neither
   child nor parent is removed and failed operations do not partially persist.

Close both GUIs after operations settle. If any case differs, record the exact
case/result; do not keep repeating inverse operations or repair/reset anything.

### 9. FINAL CHECKPOINT — required before reporting Windows complete

```bash
scripts/phase12/b77/04-checkpoint.sh --approved --platform windows \
   --qa-dir "$qaDirectory" --label final
```

Expected: all original rows/history unchanged, only prefixed QA additions, valid
ownership/graph/audit/outbox/final grants/health, sealed final inventory. Preserve
all QA records. Report each numbered section, every checkpoint PASS and any omitted
or failed case. Native connection-loss/permission-denial/oversized-input cases
not performed remain NOT EXECUTED; offline tests are separate evidence. Do not
perform service shutdown, SQL KILL, privilege changes, resets, restores or cleanup.

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
