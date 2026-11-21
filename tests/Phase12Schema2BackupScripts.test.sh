#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Synthetic clients/data/dumper only: never connects or creates a real backup.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_B78_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12GuiAcceptanceScripts.test.sh"
trap 'printf "FAIL: B7.8 fixture line %s.\n%s\n" "$LINENO" "${result//${MOCK_SECRET}/[fixture-secret]}" >&2' ERR
# Extend permission mocks for the new capture's file-level checks.
cat > "$temporaryDirectory/bin/stat" <<'STAT'
#!/usr/bin/env bash
if [[ -d "${@: -1}" ]]; then printf '%s:700\n' "$EUID"
else printf '%s:%s\n' "$EUID" "${MOCK_FILE_MODE:-600}"; fi
STAT
cat > "$temporaryDirectory/bin/powershell.exe" <<'ACL'
#!/usr/bin/env bash
[[ "${MOCK_ACL_FAIL:-0}" != 1 ]]
[[ "${TASKCOMPASS_ACL_OPERATION:-}" != create || "${MOCK_NEW_ACL_FAIL:-0}" != 1 ]]
ACL
chmod +x "$temporaryDirectory/bin/stat" "$temporaryDirectory/bin/powershell.exe"
run78() { bash "$repositoryDirectory/scripts/phase12/b78/$1.sh" --platform "$platform" --evidence-dir "$snapshot" --qa-dir "$MOCK_QA_RUN" --client "$temporaryDirectory/bin/mariadb" --build-dir "$temporaryDirectory/build with spaces" --dump "$temporaryDirectory/bin/mariadb-dump" --python "$TASKCOMPASS_TEST_PYTHON" "${@:2}"; }
cat > "$temporaryDirectory/bin/mariadb-dump" <<'DUMP'
#!/usr/bin/env bash
set -euo pipefail
if [[ " $* " == *' --version '* ]]; then cat "$MOCK_SNAPSHOT/dump-version.txt"; exit 0; fi
[[ "${@: -1}" == taskcompass_dev && "$MYSQL_PWD" == "$MOCK_SECRET" && " $* " != *' --password '* ]]
for required in --no-defaults --single-transaction --skip-lock-tables --no-tablespaces --skip-triggers --skip-add-drop-table --skip-add-locks --skip-disable-keys --no-create-db --hex-blob; do [[ " $* " == *" $required "* ]]; done
output=''
for arg in "$@"; do [[ "$arg" != --result-file=* ]] || output=${arg#*=}; done
[[ -n "$output" && ! -e "$output" ]]
printf 'dump\n' >> "$MOCK_DIRECTORY/calls"
if [[ "${MOCK_DUMP_INTERRUPT:-0}" == 1 ]]; then
 printf 'partial\n' > "$output"; kill -TERM "$PPID"; exit 1
fi
if [[ "${MOCK_DUMP_EMPTY:-0}" == 1 ]]; then : > "$output"; exit 0; fi
if [[ "${MOCK_DUMP_FAIL:-0}" == 1 ]]; then printf 'partial\n' > "$output"; exit 1; fi
"$TASKCOMPASS_TEST_PYTHON" - "$output" <<'PY'
import importlib.util, os, sys
from pathlib import Path
spec=importlib.util.spec_from_file_location('dumpFixture', Path(os.environ['MOCK_REPOSITORY'])/'tests/Phase12Schema2BackupTests.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
rows=m.CHECK.evidence.readExport(os.environ['MOCK_QA_CURRENT'],m.CHECK.MANIFEST)
data={name:[fields for _,fields in table.values()] for name,table in rows.items()}
Path(sys.argv[1]).write_bytes(m.dump(data).encode('utf-8'))
if os.environ.get('MOCK_DUMP_DRIFT')=='1':
   p=Path(os.environ['MOCK_QA_CURRENT']);p.write_bytes(p.read_bytes().replace(b'4F726967696E616C', b'4368616E676564'))
if os.environ.get('MOCK_DUMP_UNSAFE')=='1':
   with Path(sys.argv[1]).open('ab') as stream:stream.write(b'DROP TABLE Task;\n')
PY
DUMP
chmod +x "$temporaryDirectory/bin/mariadb-dump"
for platform in linux windows; do
 export MSYSTEM=''; [[ "$platform" != windows ]] || export MSYSTEM=UCRT64
 setup75 "schema2 backup $platform"
 for stage in 02-migration-grants 03-migrate 04-runtime-grants 05-verify; do result=$(passwords75 | run75 "$stage" --approved 2>&1); done
 export MOCK_SNAPSHOT="$snapshot" MOCK_QA_CURRENT="$snapshot.current" MOCK_QA_RUN="$snapshot/b77-$platform-fixture"
 mkdir -- "$MOCK_QA_RUN"
 export MOCK_QA_DATASET=21be9da7-b767-4578-b972-cc9a35122ddb
 [[ "$platform" != windows ]] || export MOCK_QA_DATASET=457bb52b-fea4-4347-9c47-675c6b035c90
 "$TASKCOMPASS_TEST_PYTHON" - <<'PY'
from pathlib import Path
import importlib.util, os
spec=importlib.util.spec_from_file_location('fixture', Path(os.environ['MOCK_REPOSITORY'])/'tests/GuiAcceptanceEvidenceTests.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.DATASET=os.environ['MOCK_QA_DATASET']
root=Path(os.environ['MOCK_SNAPSHOT']);qa=Path(os.environ['MOCK_QA_RUN']);m.PREFIX='B77-QA-'+qa.name[4:]
before=m.baseline()
if 'linux' in qa.name:
 old=before['Task'][0]['id'];new='24dd11c3-59df-4e23-ac21-422bcf2a1e3a';before['Task'][0]['id']=new
 for table in ('CommandRecord','LocalChange'):
  for row in before[table]:
   if row.get('taskId')==old:row['taskId']=new
for path in [root/'b75-schema2.hex.tsv',root/'b75-final.hex.tsv',qa/'data.hex.tsv']:path.write_bytes(m.export(before).encode('utf-8'))
current=m.qaRecords(before)
if 'linux' in qa.name:
 current['Task'][0]['objectiveId']=m.identity(12);current['Task'][0]['localRevision']=2;current['Task'][0]['updatedAt']=2
Path(os.environ['MOCK_QA_CURRENT']).write_bytes(m.export(current).encode('utf-8'))
(qa/'prefix.txt').write_bytes((m.PREFIX+'\n').encode('utf-8'))
for name in ['health.tsv','inventory.tsv','protected-inventory.tsv','qa-names.tsv']:(qa/name).write_bytes(b'fixture\n')
for label in ['hierarchy','ownership-moves','before-restart','after-restart','final']:
 p=qa/label;p.mkdir();(p/'data.hex.tsv').write_bytes(m.export(current).encode('utf-8'))
 for name in ['health.tsv','inventory.tsv']:(p/name).write_bytes(b'fixture\n')
PY
 # The B7.7 receipt is the B7.5 marker plus its final grant hashes/platform target.
 receipt=$(cat "$snapshot/b75-runtime-success.txt")
 printf '%s\n' "$receipt" > "$MOCK_QA_RUN/receipt.txt"
 (cd -- "$snapshot"
  sha256sum b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv > b75-preservation.sha256
  sha256sum b75-verification-success.txt b75-final-health.tsv b75-final.hex.tsv > b75-final.sha256)
 (cd -- "$MOCK_QA_RUN";sha256sum prefix.txt receipt.txt data.hex.tsv health.tsv inventory.tsv protected-inventory.tsv qa-names.tsv > initial.sha256)
 if [[ "$platform" == windows ]]; then
  for label in hierarchy ownership-moves before-restart after-restart final; do (cd -- "$MOCK_QA_RUN/$label";sha256sum data.hex.tsv health.tsv inventory.tsv > checkpoint.sha256); done
 fi
 initialCalls=$(wc -l < "$MOCK_DIRECTORY/calls")
 for stage in 01-check 02-preflight 03-capture 04-review; do
  run78 "$stage" --print-plan >/dev/null
  [[ "$stage" == 01-check ]] || if result=$(run78 "$stage" 2>&1); then exit 1; fi
 done
 [[ "$(wc -l < "$MOCK_DIRECTORY/calls")" == "$initialCalls" ]]
 run78 01-check >/dev/null
 mutations=$(countDevMutations "$MOCK_DIRECTORY/calls")
 result=$(passwords75 | run78 02-preflight --approved 2>&1)
 for mode in DEV_WRONG_SERVER DEV_WRONG_UUID DEV_EXTRA_GRANT DEV_BAD_HEALTH; do
  export "$mode=1"
  if result=$(passwords75 | run78 02-preflight --approved 2>&1); then exit 1; fi
  unset "$mode"
 done
 if result=$(passwords75 | run78 03-capture --approved --parent "$snapshot/nested-backup" 2>&1); then exit 1; fi
 [[ ! -e "$snapshot/nested-backup" ]]
 if result=$(passwords75 | run78 03-capture --approved --parent "$repositoryDirectory/temp/b78-must-not-create" 2>&1); then exit 1; fi
 [[ ! -e "$repositoryDirectory/temp/b78-must-not-create" ]]
 if [[ "$platform" == windows ]]; then
  export MOCK_NEW_ACL_FAIL=1
  if result=$(passwords75 | run78 03-capture --approved --parent "$temporaryDirectory/acl failure" 2>&1); then exit 1; fi
  unset MOCK_NEW_ACL_FAIL
  [[ -z "$(find "$temporaryDirectory/acl failure" -type f -print)" ]]
 fi

 export DEV_ACTIVE_SESSION=1
 if result=$(passwords75 | run78 03-capture --approved --parent "$temporaryDirectory/blocked $platform" 2>&1); then exit 1; fi
 unset DEV_ACTIVE_SESSION
 [[ ! -e "$temporaryDirectory/blocked $platform" ]]
 result=$(passwords75 | run78 03-capture --approved --parent "$temporaryDirectory/new backups $platform" 2>&1)
 capture=${result##*Directory: };capture=${capture%%$'\n'*}
 [[ -s "$capture/capture.sha256" && ! -e "$capture/schema2-evidence.sha256" ]]
 [[ "$platform" != linux || "$(cat "$capture/preservation.txt")" == *UNRESOLVED* ]]
 if result=$(run78 04-review --approved --snapshot-dir "$capture" 2>&1); then exit 1; fi
 [[ ! -e "$capture/schema2-evidence.sha256" ]]
 result=$(run78 04-review --approved --snapshot-dir "$capture" --dump-reviewed 2>&1)
 [[ "$result" == *'snapshot sealed'* && -s "$capture/schema2-evidence.sha256" ]]
 (cd -- "$capture"; sha256sum --check schema2-evidence.sha256 >/dev/null)
 [[ "$result" != *"$MOCK_SECRET"* ]]
 # Tampering in a separate unsealed temporary fixture must never be sealed.
 tampered="$temporaryDirectory/Phase1.2-schema2-$platform-tampered"
 mkdir -- "$tampered"; cp "$capture/"* "$tampered/"
 rm -- "$tampered/schema2-evidence.sha256" "$tampered/private-review.txt"
 printf 'tamper\n' >> "$tampered/schema2.sql"
 if result=$(run78 04-review --approved --snapshot-dir "$tampered" --dump-reviewed 2>&1); then exit 1; fi
 [[ ! -e "$tampered/schema2-evidence.sha256" ]]
 interruptedReview="$temporaryDirectory/Phase1.2-schema2-$platform-interrupted-review"
 mkdir -- "$interruptedReview"; cp "$capture/"* "$interruptedReview/"
 rm -- "$interruptedReview/schema2-evidence.sha256"
 # The existing review receipt represents interruption before sealing, never replay it.
 if result=$(run78 04-review --approved --snapshot-dir "$interruptedReview" --dump-reviewed 2>&1); then exit 1; fi
 [[ ! -e "$interruptedReview/schema2-evidence.sha256" && -s "$interruptedReview/private-review.txt" ]]


 if result=$(run78 04-review --approved --snapshot-dir "$capture" --dump-reviewed 2>&1); then exit 1; fi
 for mode in MOCK_DUMP_FAIL MOCK_DUMP_UNSAFE MOCK_DUMP_EMPTY MOCK_DUMP_INTERRUPT MOCK_DUMP_DRIFT; do
  export "$mode=1"
  if result=$(passwords75 | run78 03-capture --approved --parent "$temporaryDirectory/failure $mode $platform" 2>&1); then exit 1; fi
  unset "$mode"
  cp -- "$MOCK_QA_RUN/final/data.hex.tsv" "$MOCK_QA_CURRENT"
  failed=$(find "$temporaryDirectory/failure $mode $platform" -name attempt.txt -print)
  [[ -n "$failed" && ! -e "${failed%/*}/capture-complete.txt" && ! -e "${failed%/*}/schema2-evidence.sha256" ]]
 done
 [[ "$mutations" == "$(countDevMutations "$MOCK_DIRECTORY/calls")" ]]
 printf 'PASS: B7.8 %s fake preflight/capture/review, full dump bytes, private evidence, no overwrite, active-writer/partial/unsafe refusal and no DB writes.\n' "$platform"
done
