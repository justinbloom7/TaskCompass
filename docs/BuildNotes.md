<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Automatic environment and build notes

Use the same reporter on Linux Bash and Windows MSYS2 UCRT64:

```bash
scripts/environment-report.sh
```

It writes machine-local notes **inside the repository's ignored build directory**:

- Linux: `build/validation-notes/BuildNotes.md`.
- Windows: `build-ucrt64/validation-notes/BuildNotes.md`.
- Every invocation also preserves a unique timestamped `environment-*.json` snapshot.

The Markdown notes append a dated before/after entry whenever recorded environment,
source or desktop-binary fields change. Unchanged runs still get a snapshot but do
not repeat the change history. Files are not overwritten or automatically committed.
No credentials, Git remotes, raw diffs, task content or environment-variable dumps
are collected. Reports use installed tools with --version, selected CMake cache
fields and project source hashes; they never query a database.

Recorded fields: OS/kernel/architecture, MSYS2 environment, CMake, compiler selected
by CMake, generator/build type, QtCore/Widgets package versions selected by the
build cache, full Git revision/branch/dirty state, project-source fingerprint and
TaskCompass executable hash. Source fingerprints include tracked/untracked project
code, scripts, tests, migrations/docs and top-level project files; ignored build/
temporary/private files are excluded. Dirty source changes are detected even when
the Git commit remains the same. This describes the current checkout and executable
hash, not proof that an unbuilt source edit is already compiled.

Qt version comes from the CMake-selected installed package version files, not a
claim about which runtime DLL a running Windows process loaded. Missing tools,
unconfigured builds and unbuilt executables are labeled unavailable, never guessed.
A build-start report is emitted before configure/build, and a build-output report
after successful compilation, before CTest. Reports do not claim tests passed.

## Server metadata and GUI acceptance

Standalone reports say `server: not queried (offline)` by default. An existing
verified metadata file can be attached without logging into MariaDB again:

```bash
scripts/environment-report.sh --build-dir build \
   --server-evidence "$qaDirectory/../b75-final-grants.txt" \
   --snapshot-dir "$qaDirectory"
```

Use build-ucrt64 on Windows. This command works with an already-open GUI because
it reads saved files only. The QA directory must already be the actual private path
printed by preflight. It copies a new JSON snapshot and checksum alongside the QA
files, preserving all prior reports and seals. Saved metadata is explicitly labeled
with its provenance; it is not presented as a new server query. A changed provenance
filename alone does not count as an environment/version change.

B7.7 preflight and checkpoint scripts automatically attach server identity from
their **existing freshly checked** root metadata. Launch attaches the saved B7.5
verified server identity; it adds no root/password request or SQL. All three stages
print the report and copy snapshots/checksums into the private QA run directory.
Existing QA directories/initial seals remain compatible; no preflight rerun or
GUI restart is necessary just to collect this information.

The shared Phase1.2 build script (and B7.7 build wrapper) calls the reporter
implicitly. G01 therefore no longer asks the operator to write down known versions.
Record only manual observations/defects; the scripts collect version evidence.
Future validation scripts should reuse this helper as recorded in AGENTS.md.

## Options and review

`--build-dir`, `--output-dir`, `--context`, `--server-evidence` and `--snapshot-dir`
are optional. `--output-dir` selects the local report-history directory; do not point
it at a sealed capture to overwrite its role. `--snapshot-dir` adds individual new
sidecar files without creating nested report directories inside a QA run.
`TASKCOMPASS_REPORT_PYTHON` may select an existing trusted Python3 executable.
Python3 is already required by the repository's documentation/test tooling. No
script installs dependencies, connects to a server, changes grants, launches a GUI
or modifies the database. Share reports only after ordinary privacy review of
machine/source paths; no secret is intentionally included.
