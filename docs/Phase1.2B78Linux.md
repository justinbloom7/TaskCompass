<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# B7.8 Linux — numbered operator steps

Prepared only. Read [scope, evidence and failure policy](Phase1.2B78.md).
Existing Linux preservation/restart exceptions remain unresolved. Do not run the
live commands until their specific authorization is recorded.

1. Offline build/test and tool check (no MariaDB connection):

```bash
scripts/phase12/b77/01-build.sh
scripts/phase12/b78/01-check.sh --platform linux
```

Expected: offline tests PASS, two live tests skipped, reviewed checks/tools/embedded
migration checks PASS. Do not run reset-capable integration scripts.

2. **First approval requested: Linux02 only.** Close every TaskCompass GUI and
   other writer. After that separate approval, run:

```bash
scripts/phase12/b78/02-preflight.sh --approved --platform linux
```

Passwords: Linux taskcompass_migrate, taskcompass_app, taskcompass_dev, plus sudo
OS authentication for existing socket root. Expected: exact feledria/10.5.29,
schema2/nine tables/UUID/final19 runtime rights/SELECT-only diagnostic+migrator,
zero health violations/no writers, current full data matches the retained failed
Linux final export. Explicit UNRESOLVED exception notice is expected; an additional
protected change is NOT expected. No backup directory is created. **Report output
and wait for capture approval.** If anything differs, stop; do not repair/retry.

3. **Only after separate Linux03 approval**, keep writers closed and run:

```bash
scripts/phase12/b78/03-capture.sh --approved --platform linux
```

Expected: fresh directory
`$HOME/temp/TaskCompassBackups/Phase1.2-schema2-linux-YYYYmmdd-HHMMSS-PID`, mode700,
files600, one exit0 dump, all nine-table data/raw DDL/health/grant comparisons and
capture checksums PASS. End message says PRIVATE REVIEW PENDING and prints the
complete new Directory. Never reuse the schema1/QA directory as this destination.
Preserve any failed directory without retry. No old files or database rows change.

4. Copy the actual new directory printed by03 (this prompt requests real missing
   information, not approval). Privately inspect the dump before requesting/running
   the offline sealing stage:

```bash
IFS= read -r -p 'Paste the complete NEW schema2 directory printed by stage03: ' snapshotDirectory
LESSHISTFILE=- LESSSECURE=1 less -- "$snapshotDirectory/schema2.sql"
```

Read every SQL statement against the guide, then quit less with q. If review finds
anything unexpected, stop and report without sharing private task values or sealing.

5. **After Linux04 review/sealing authorization and completed private review:**

```bash
scripts/phase12/b78/04-review.sh --approved --platform linux --snapshot-dir "$snapshotDirectory" --dump-reviewed
```

Expected: capture hashes, old-reference hashes, complete dump/data/raw DDL/health
checks and private file checks PASS; schema2-evidence.sha256 created/verified.
Record its path and schema2.sql hash privately. Linux exceptions still remain
UNRESOLVED. Report result for review; do not start Windows, restore or cleanup.
