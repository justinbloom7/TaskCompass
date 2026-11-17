<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Phase 1.2 B7.2 — approved operator-only schema-1 capture

Owner approved B7.2 on Linux and Windows on 2026-11-17. Use the numbered scripts
below instead of pasting the earlier manual blocks. They run the same reviewed
[B7.2 procedure](Phase1.2B7.md#b72--fresh-populated-schema-1-backup-and-evidence),
including its existing Phase1.1 private schema-1 backup helper. The unchanged B7
review package retains the underlying manual commands/SQL. Approvals, prior
instructions and actual results remain in [Phase1.2.md](milestones/Phase1.2.md).

Run **one command at a time**, from repository root, on each platform separately.
Linux uses Bash; Windows uses **MSYS2 UCRT64**. Transfer the latest scripts through
the owner's normal source workflow first. Close all TaskCompass clients and SQL
writers; keep MariaDB running. Keep writers closed throughout. TaskCompass password entry allows up to three
confirmed authentication-denial attempts before any capture begins; this is not
a retry of a backup/export. Stop/report any operation failure or discrepancy;
never automatically rerun capture, repair, reset or clean up artifacts.

## 1 — tool and reviewed-file checks (no database connection)

Same command on Linux and Windows:

```bash
scripts/phase12/b72/01-check.sh
```

Expected: reviewed checksums OK, native client/dump versions, built verifier and
required review/ACL tools available, `PASS: offline B7.2 tools/checksums`.
No directory or backup is created. Missing tools stop; nothing is installed.

## 2 — fresh read-only baseline and grants

Same command on Linux and Windows:

```bash
scripts/phase12/b72/02-preflight.sh --approved
```

Linux automatically uses local socket/sudo (Linux OS password if requested).
Windows uses the native MariaDB client over TCP and requests the **Windows MariaDB
root** password. No administrator password is captured by the script.

The script executes the reviewed SELECT/SHOW files and prints expected baseline
summary. **Run this immediately before step 3 and review the report before continuing.** Exit 0 means queries ran,
not an automatic certification that every value/grant is unchanged. Capture
requires your explicit unchanged-baseline confirmation; no persistent preflight
receipt is saved or blindly sourced.

| Expected baseline | Linux | Windows |
| --- | --- | --- |
| Server/version | feledria / 10.5.29 | feldria-w10 / 13.0.2 |
| Dataset UUID | 21be9da7-b767-4578-b972-cc9a35122ddb | 457bb52b-fea4-4347-9c47-675c6b035c90 |
| Task / CommandRecord / LocalChange | 4 / 28 / 28 | 2 / 16 / 16 |
| Task groups | 2 active, 1 active tombstone, 1 completed | 2 active, no tombstones |
| Migration appliedAt / Dataset createdAt | 1794627756442 / 1794627756441 | 1794691232396 / 1794691232396 |

Both: port3306, taskcompass_dev, utf8mb4/utf8mb4_unicode_ci, exactly five known
InnoDB tables, one Dataset and one SchemaMigration row (version1), no stored code
or other selected-target sessions. Windows table-name normalization is expected.
Migration1 checksum bb901d7580398893b5c04ee263f813b9d05873eb4bae08e0daf377faf73b0ea0;
stored fingerprint 6b3bd320ea92630b21909de0bd7cd38ea3bc3f644a69ba1fd249db43e656a343.

Grants: global USAGE only; diagnostic/migrator schema SELECT only; app exactly 9
table rights (Task SELECT/INSERT/UPDATE; CommandRecord/LocalChange SELECT/INSERT;
Dataset/SchemaMigration SELECT). No extra roles, column rights, grant option or
PUBLIC authority covering TaskCompass. Unrelated Windows PUBLIC test/test\_% rights
remain unchanged. Any difference requires review before proceeding. Counts alone
are not all-value history equality; complete quiescent exports follow.

## 3 — fresh schema-1 verification, private backup and evidence

Only after step 2 is unchanged:

```bash
scripts/phase12/b72/03-capture.sh --approved
```

No environment setup or directory assignment is needed. It automatically chooses
a **new** independently timestamped directory:

- Linux: `$HOME/temp/TaskCompassBackups/Phase1.2-schema1-linux-<timestamp>`.
- Windows: `$HOME/TaskCompassBackups/Phase1.2-schema1-windows-<timestamp>`.

Confirm the reviewed baseline with this exact phrase when prompted:

```text
BASELINE UNCHANGED; WRITERS CLOSED
```

The existing helper additionally requires its established confirmation:

```text
ALL TASKCOMPASS WRITERS CLOSED
```

TaskCompass credential entry now has **up to three attempts per account** and
shows `[linux]` or `[windows]`. On a normal run enter each platform's password once,
in order: taskcompass_migrate, taskcompass_app, taskcompass_dev. Successfully checked
passwords stay only in the current shell process's memory and are reused for the
helper, exports and final verification. All three are checked before creating the
backup directory. No late migrator or diagnostic re-entry is needed. The existing
helper receives the explicit writer confirmation and checked credentials through
an anonymous pipe; no password appears in arguments, saved files, history or logs.
The cache is cleared on exit and is not shared with another command or platform.
Linux sudo and Windows native administrator-password handling in step2 are unchanged;
no administrator secret is captured/cached by these scripts.

Only the read-only native login/identity probe may repeat, and only for recognized
ERROR1045/SQLSTATE28000 authentication denial (or empty entry), bounded to three
entries. EOF cancels. Network, plugin, permission, identity, schema and unknown
errors stop; a Qt verification failure after native authentication is not retried.
No dump, export, sealing, migration or grant operation is ever retried. Known client
password-option warnings are explained without changing TLS verification settings.

The script verifies pinned schema1/known UUID before and after capture; delegates
the private dump to existing 09-backup (umask077/Linux, protected owner ACL/Windows
before files); checks nonempty dump and five-table source before/after equality;
exports all 38 original columns/history/tombstones twice with matching hashes;
records raw DDL and native versions; seals and verifies all evidence checksums.
All destinations refuse overwrite. Separate captures require stopped writers;
they are not claimed to be one server transaction. No SQL writes/DDL are executed.

Expected: schema1/same platform UUID, helper success, every checksum OK and final
`PASS: B7.2 capture, comparisons and checksums`. Save the printed `Directory:` path.
Private SQL review is still pending. Any failure stops, leaves all evidence and
must be reported; no automatic retry/recovery/cleanup or replacement capture.

## If step 3 stops at final verification

Do not rerun capture or try review against an unsealed directory. Preserve files
and report the exact failure. A separate `03a-finish-capture.sh` exists only for
an explicitly reviewed final-authentication/verification failure after successful
dump/exports. It is **not part of the normal four-step sequence** and requires new
owner approval for the exact existing directory. See the 2026-11-17 failure report
and **Approval Requested** section in the milestone before invoking it.

It refuses missing/linked/inconsistent evidence or already-created completion
metadata, checks private permissions and original hashes/comparisons, compares
current legacy-data hash to the capture without writing an export, verifies1, then
writes only missing client-version.txt, dump-version.txt and checksum manifest.
No dump/export replacement, migration, restore, grant, reset or cleanup. Any
interruption or partial metadata creation stops again for explicit review; there
is no automatic resume/repair. Targeted completion of the retained Linux directory ending
`Phase1.2-schema1-linux-20261117-013703` was explicitly approved for owner execution
on 2026-11-17; exact commands are in the milestone. This does not approve completion
of other failed captures. No actual completion has been executed by the agent.

## 4 — checksum recheck and private SQL review

```bash
scripts/phase12/b72/04-review.sh
```

Paste the **plain directory path** printed by step 3 when asked (no surrounding
quotes). This stage checks private directory permissions, expected-file checksum
coverage/paths, hashes, UUID and both comparisons. It then opens the dump in less;
quit with `q`. No database connection or SQL execution occurs in this stage.

Review the entire file: five expected table definitions, indexes/constraints,
INSERT data and normal session-only prologue/epilogue. Dataset UUID must match.
Reject database/account/grant statements, DROP/REPLACE/TRUNCATE, GLOBAL changes,
stored code/DEFINER, OUTFILE or source-qualified write redirection. Normal session
FK-check save/disable/restore is expected dump content, not permission to import.
Only after the whole dump checks out, type:

```text
DUMP REVIEWED
```

Expected: `PASS: B7.2 checksums/comparisons and owner-confirmed private review`.
This records your confirmation in terminal output; it does not automatically prove
SQL safety, create a receipt or authorize restore. Do not edit the dump to pass.

Share only platform, fresh preflight outcome, native versions, directory path,
exit/PASS/checksum results, after-gui.sql SHA-256, UUID/count summary and private
review outcome. No dumps/exports/HEX payloads/passwords in chat or Git. Keep all
artifacts and restored databases. **Stop here; B7.3 requires separate approval.**

## Options and validation boundaries

All stages support `--help` / `--print-plan` without connection or capture. Stages
2/3 refuse connections without `--approved`. Current B7.2 approval covers only the
operator actions above, not a later stage. Scripts never invoke migration apply,
imports, resets, provisioning, grants, GUI writes or artifact cleanup.

Optional overrides for installed tools/locations: `--client PATH`, `--dump PATH`,
`--build-dir PATH`. Capture optionally accepts `--backup-dir NEW_PATH` outside the
repository within the approved backup location (a different location requires
owner approval); review accepts `--backup-dir EXISTING_PATH` instead of its prompt.
Pass paths with spaces quoted. Overrides do not allow another database/user/host.
Default targets are fixed taskcompass_dev, loopback3306 and the known platform UUID.
Do not change them to force success or silently choose a different server/version.

Scripts were tested with **offline fake executables** (including a mocked Windows
path/ACL flow). Actual live script execution on both platforms remains pending;
these are not new capture or recovery results. Earlier manual command design and
its syntax validation remain historical in the milestone. No real procedure has
been executed by the agent. Backup success is not restore proof.
