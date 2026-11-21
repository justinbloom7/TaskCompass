<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.8 Windows UCRT64 — numbered operator steps

Prepared only; **Windows live stages are NOT authorized by Linux approval**.
Wait for Linux capture/private review results and independent Windows stage approval.
Read [scope and failure policy](Phase1.2B78.md); use Windows passwords and paths.

1. Native offline validation in UCRT64 (no live database connection):

```bash
scripts/phase12/b77/01-build.sh
scripts/phase12/b78/01-check.sh --platform windows
```

Expected: offline suite PASS/two live skips. New Windows package behavior needs
native validation; Linux mocks are not a native pass. Stop/report any failure.

2. Close every TaskCompass window and other writer. **After Windows02 approval:**

```bash
scripts/phase12/b78/02-preflight.sh --approved --platform windows
```

Expected: feldria-w10/MariaDB13.0.2, taskcompass_dev@127.0.0.1:3306, dataset
457bb52b-fea4-4347-9c47-675c6b035c90, schema2/nine InnoDB tables/final grants/zero
health/no writers. All protected rows unchanged, all five B7.7 seals valid,
source matches final and restart comparison preserved. Root prompt is the Windows
MariaDB administrator password, never sudo. Report PASS for stage03 approval.

3. **After separate Windows03 approval**, keep writers stopped:

```bash
scripts/phase12/b78/03-capture.sh --approved --platform windows
```

Expected: NEW
`$HOME/TaskCompassBackups/Phase1.2-schema2-windows-YYYYmmdd-HHMMSS-PID`, current-user
only ACL before files are created, all file ACLs verified. Native result-file path
is converted automatically; do not use a Linux path. One exit0/nonempty dump,
nine-table values/raw DDL/health/grant comparisons/checksums PASS; PRIVATE REVIEW
PENDING. Preserve the printed full Directory and any failed artifacts.

4. Privately inspect every statement in the new dump, using its actual path:

```bash
IFS= read -r -p 'Paste the complete NEW Windows schema2 directory printed by stage03: ' snapshotDirectory
LESSHISTFILE=- LESSSECURE=1 less -- "$snapshotDirectory/schema2.sql"
```

No SQL is executed. Compare against the guide; q exits less. Unexpected SQL means
STOP, no sealing, no permissions changes to make it work.

5. **After Windows04 authorization and completed private SQL review:**

```bash
scripts/phase12/b78/04-review.sh --approved --platform windows --snapshot-dir "$snapshotDirectory" --dump-reviewed
```

Expected: owner-reviewed schema2-evidence.sha256 created and verified; all capture
files, timestamped environment sidecars and private-review attestation covered.
Report reviewed snapshot path/hash and result. Preserve all QA/rehearsal/old backup
artifacts. No account/grant change, restore/reset, cleanup or Phase1.3 follows.
