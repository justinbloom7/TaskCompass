#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Fake database clients and executable only; never opens a GUI or live connection.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_B75_INSPECTION_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12DevelopmentUpgradeScripts.test.sh"
cp "$temporaryDirectory/bin/mariadb" "$temporaryDirectory/bin/b77-old-client"
cat > "$temporaryDirectory/bin/mariadb" <<'CLIENT'
#!/usr/bin/env bash
set -euo pipefail
if [[ " $* " == *' --version '* || " $* " == *' --user=root '* || " $* " == *' --database=taskcompass_phase12_test '* || ! -v MOCK_QA_CURRENT ]]; then
 exec "$MOCK_DIRECTORY/bin/b77-old-client" "$@"
fi
sql=$(cat)
if [[ "$sql" == *"'COUNT:MigrationAttempt'"* ]]; then
 [[ "$MYSQL_PWD" == "$MOCK_SECRET" && " $* " == *' --database=taskcompass_dev '* && " $* " == *' --user=taskcompass_dev '* ]]
 cat "$MOCK_QA_CURRENT"
else exec "$MOCK_DIRECTORY/bin/b77-old-client" "$@" <<< "$sql"; fi
CLIENT
cat > "$temporaryDirectory/build with spaces/TaskCompass" <<'GUI'
#!/usr/bin/env bash
set -euo pipefail
[[ "$TASKCOMPASS_DB_NAME" == taskcompass_dev && "$TASKCOMPASS_DB_USER" == taskcompass_app && "$TASKCOMPASS_DB_HOST" == 127.0.0.1 && "$TASKCOMPASS_DB_PORT" == 3306 && "$TASKCOMPASS_DB_PASSWORD" == "$MOCK_SECRET" && "$TASKCOMPASS_DB_ISOLATED" == 1 ]]
printf 'fake-GUI\n' >> "$MOCK_DIRECTORY/calls"
if [[ "${MOCK_GUI_WRITE:-0}" == 1 ]]; then
 "$TASKCOMPASS_TEST_PYTHON" - <<'PY'
import importlib.util, os
from pathlib import Path
spec=importlib.util.spec_from_file_location('fixture',Path(os.environ['MOCK_REPOSITORY'])/'tests/GuiAcceptanceEvidenceTests.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
m.DATASET=os.environ['MOCK_QA_DATASET'];m.PREFIX=Path(os.environ['MOCK_QA_RUN']+'/prefix.txt').read_text().strip()
Path(os.environ['MOCK_QA_CURRENT']).write_text(m.export(m.qaRecords(m.baseline())),encoding='utf-8')
PY
fi
GUI
chmod +x "$temporaryDirectory/bin/mariadb" "$temporaryDirectory/build with spaces/TaskCompass"
for platform in linux windows; do
 unset MSYSTEM MOCK_QA_CURRENT
 [[ "$platform" != windows ]] || export MSYSTEM=UCRT64
 setup75 "GUI $platform"
 for stage in 02-migration-grants 03-migrate 04-runtime-grants 05-verify; do result=$(passwords75 | run75 "$stage" --approved 2>&1); done
 export MOCK_QA_DATASET=21be9da7-b767-4578-b972-cc9a35122ddb
 [[ "$platform" != windows ]] || export MOCK_QA_DATASET=457bb52b-fea4-4347-9c47-675c6b035c90
 export MOCK_QA_CURRENT="$snapshot.qa-current"
 "$TASKCOMPASS_TEST_PYTHON" - <<'PY'
import importlib.util, os
from pathlib import Path
spec=importlib.util.spec_from_file_location('fixture',Path(os.environ['MOCK_REPOSITORY'])/'tests/GuiAcceptanceEvidenceTests.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.DATASET=os.environ['MOCK_QA_DATASET']
Path(os.environ['MOCK_QA_CURRENT']).write_text(m.export(m.baseline()),encoding='utf-8')
PY
 cp "$MOCK_QA_CURRENT" "$snapshot/b75-schema2.hex.tsv"
 cp "$MOCK_QA_CURRENT" "$snapshot/b75-final.hex.tsv"
 (cd -- "$snapshot"
  sha256sum b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv > b75-preservation.sha256
  sha256sum b75-verification-success.txt b75-final-health.tsv b75-final.hex.tsv > b75-final.sha256)
 helperDirectory="$repositoryDirectory/scripts/phase12/b77"
 calls=$(wc -l < "$MOCK_DIRECTORY/calls")
 for stage in 02-preflight 03-launch 04-checkpoint; do
  run75 "$stage" --print-plan >/dev/null
  if result=$(run75 "$stage" 2>&1); then exit 1; fi
 done
 [[ "$calls" == "$(wc -l < "$MOCK_DIRECTORY/calls")" ]]
 result=$(passwords75 | run75 02-preflight --approved 2>&1)
 [[ "$result" == *'PASS: B7.7 pristine'* ]]
 runs=("$snapshot"/b77-*)
 export MOCK_QA_RUN=${runs[0]}
 export MOCK_GUI_WRITE=1
 result=$(passwords75 | run75 03-launch --approved --qa-dir "$MOCK_QA_RUN" 2>&1)
 unset MOCK_GUI_WRITE
 [[ "$result" == *'GUI exited0; manual acceptance is not inferred.'* && "$result" != *"$MOCK_SECRET"* ]]
 result=$(passwords75 | run75 04-checkpoint --approved --qa-dir "$MOCK_QA_RUN" --label hierarchy 2>&1)
 [[ "$result" == *'checkpoint hierarchy sealed'* ]]
 result=$(passwords75 | run75 03-launch --approved --qa-dir "$MOCK_QA_RUN" 2>&1)
 result=$(passwords75 | run75 04-checkpoint --approved --qa-dir "$MOCK_QA_RUN" --label restart --compare hierarchy 2>&1)
 [[ "$result" == *'checkpoint restart sealed'* ]]
 if result=$(passwords75 | run75 04-checkpoint --approved --qa-dir "$MOCK_QA_RUN" --label hierarchy 2>&1); then exit 1; fi
 export DEV_ACTIVE_SESSION=1
 if result=$(passwords75 | run75 04-checkpoint --approved --qa-dir "$MOCK_QA_RUN" --label blocked 2>&1); then exit 1; fi
 unset DEV_ACTIVE_SESSION
 [[ ! -e "$MOCK_QA_RUN/blocked" ]]
 # Change only a fake pre-existing title: must preserve failed checkpoint and block GUI.
 sed 's/4F726967696E616C/4368616E676564/' "$MOCK_QA_CURRENT" > "$snapshot.wrong"
 cp "$snapshot.wrong" "$MOCK_QA_CURRENT"
 if result=$(passwords75 | run75 04-checkpoint --approved --qa-dir "$MOCK_QA_RUN" --label unsafe 2>&1); then exit 1; fi
 [[ -d "$MOCK_QA_RUN/unsafe" && ! -e "$MOCK_QA_RUN/unsafe/checkpoint.sha256" ]]
 if result=$(passwords75 | run75 03-launch --approved --qa-dir "$MOCK_QA_RUN" 2>&1); then exit 1; fi
 printf 'PASS: %s fake GUI package gates, B7.5 seals, QA-only writes/inventory, restart equality, no overwrite and protected-data failure.\n' "$platform"
done
