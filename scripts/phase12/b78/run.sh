#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Operator only. All DB commands are fixed-target read-only queries/verifiers/dump.
set +x
set -Eeuo pipefail
umask 077
stage=${1:-help}; [[ $# == 0 ]] || shift
b78Directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$b78Directory/../../.." && pwd)
usage78() {
   printf 'B7.8: 01-check; 02-preflight; 03-capture; 04-review. No database writes.\n'
   printf 'All stages require --platform linux|windows; live stages require separate --approved authorization.\n'
   printf 'Review is offline: --snapshot-dir PATH --dump-reviewed attests prior private review, without typed phrases.\n'
}
case "$stage" in check|preflight|capture|review) ;; help) usage78; exit 0 ;; *) usage78 >&2; exit 1 ;; esac
approved78=false; dumpReviewed=false; selected78=''; destination=''; parent78=''; qa78=''; dump78=''; evidence78=''; python78=''
options78=()
while [[ $# -gt 0 ]]; do
   case "$1" in
      --help|--print-plan|-h) usage78; exit 0 ;;
      --approved) approved78=true; shift ;;
      --dump-reviewed) dumpReviewed=true; shift ;;
      --platform|--snapshot-dir|--parent|--qa-dir|--dump|--evidence-dir|--python|--client|--build-dir)
         [[ $# -ge 2 && -n "$2" ]] || { printf 'STOP: missing argument.\n' >&2; exit 1; }
         case "$1" in
            --platform) selected78=$2 ;; --snapshot-dir) destination=$2 ;; --parent) parent78=$2 ;;
            --qa-dir) qa78=$2 ;; --dump) dump78=$2 ;; --evidence-dir) evidence78=$2 ;;
            --python) python78=$2 ;; *) options78+=("$1" "$2") ;;
         esac; shift 2 ;;
      *) printf 'STOP: unknown option.\n' >&2; exit 1 ;;
   esac
done
case "$selected78:${MSYSTEM:-}" in linux:|windows:UCRT64) ;; *) printf 'STOP: explicit platform must match host shell.\n' >&2; exit 1 ;; esac
[[ "$stage" == check ]] || "$approved78" || { printf 'STOP: separate stage/platform --approved authorization required.\n' >&2; exit 1; }
[[ "$stage" == review || -z "$destination" ]] || { printf 'STOP: capture always creates a fresh directory.\n' >&2; exit 1; }
[[ "$stage" == capture || -z "$parent78" ]] || { printf 'STOP: --parent is capture-only.\n' >&2; exit 1; }
[[ "$stage" == review || "$dumpReviewed" == false ]] || { printf 'STOP: --dump-reviewed is review-only.\n' >&2; exit 1; }
cd -- "$repositoryDirectory"
python78=${python78:-$(command -v python3 || command -v python || true)}
[[ -n "$python78" && -x "$python78" ]] || { printf 'STOP: existing Python3 required.\n' >&2; exit 1; }
sha256sum --check scripts/phase12/b7/ReviewChecksums.sha256
sha256sum --check scripts/phase12/b78/ReviewChecksums.sha256
if [[ "$stage" == check ]]; then
   checkClient=/usr/bin/mariadb; checkBuild="$repositoryDirectory/build"
   if [[ "$selected78" == windows ]]; then
      checkClient='/c/Program Files/MariaDB 13.0/bin/mariadb.exe'; checkBuild="$repositoryDirectory/build-ucrt64"
      command -v powershell.exe >/dev/null; command -v cygpath >/dev/null
   else command -v sudo >/dev/null; fi
   for ((i=0; i<${#options78[@]}; i+=2)); do
      case "${options78[i]}" in --client) checkClient=${options78[i+1]} ;; --build-dir) checkBuild=${options78[i+1]} ;; esac
   done
   checkDump=${dump78:-/usr/bin/mariadb-dump}
   [[ "$selected78" != windows || "$checkDump" != /usr/bin/mariadb-dump ]] || checkDump='/c/Program Files/MariaDB 13.0/bin/mariadb-dump.exe'
   checkVerifier="$checkBuild/TaskCompassMigrate"; [[ ! -f "$checkVerifier.exe" ]] || checkVerifier+=.exe
   [[ -x "$checkClient" && -x "$checkDump" && -x "$checkVerifier" ]] || { printf 'STOP: required tools missing.\n' >&2; exit 1; }
   command -v sha256sum >/dev/null; command -v less >/dev/null
   env -u MYSQL_PWD "$checkClient" --no-defaults --version; env -u MYSQL_PWD "$checkDump" --no-defaults --version
   for version in 1 2; do
      actual=$("$checkVerifier" --print-migration "$version" | tr -d '\r' | sha256sum)
      migration=migrations/mariadb/001_PersistentTaskFoundation.sql
      [[ "$version" != 2 ]] || migration=migrations/mariadb/002_OrganizationalManagement.sql
      expected=$(sha256sum < "$migration")
      [[ "${actual%% *}" == "${expected%% *}" ]] || { printf 'STOP: embedded migration differs.\n' >&2; exit 1; }
   done
   TASKCOMPASS_REPORT_PYTHON="$python78" bash scripts/environment-report.sh --build-dir "$checkBuild" --context B7.8-offline-check
   printf 'PASS: offline tools/review checks only; no connection or backup.\n'; exit 0
fi

[[ -z "$evidence78" ]] || options78+=(--backup-dir "$evidence78")
# Reuse checked manifests, credential handling, schema verifier, grant/session/health rules.
# Definitions only: never replay a migration/runtime/rehearsal stage.
TASKCOMPASS_B75_LIBRARY=1 source "$b78Directory/../b75/run.sh" preflight --approved --platform "$selected78" "${options78[@]}"
trap 'fail "B7.8 failed (exit $?). Preserve partial artifacts; no retry or repair."' ERR
trap 'fail "B7.8 interrupted. Preserve actual state and artifacts."' INT TERM
for entry in \
 'b75-grants.sha256|b75-grants-attempt.txt b75-grants-success.txt b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt' \
 'b75-preservation.sha256|b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv' \
 'b75-runtime.sha256|b75-runtime-attempt.txt b75-runtime-success.txt b75-final-grants.txt' \
 'b75-final.sha256|b75-verification-success.txt b75-final-health.tsv b75-final.hex.tsv'; do
   checkManifest "${entry%%|*}" "${entry#*|}"
done
if [[ -z "$qa78" ]]; then
   if [[ "$platform" == linux ]]; then qa78="$backupDirectory/b77-linux-20261118-102657-195100"
   else qa78="$backupDirectory/b77-windows-20261118-204828-79876"; fi
fi
private78() {
   local path=$1 operation=${2:-check}
   [[ -d "$path" && ! -L "$path" ]] || fail 'Missing/linked private directory.'
   if [[ "$platform" == linux ]]; then
      [[ "$(stat -c '%u:%a' -- "$path")" == "$EUID:700" ]] || fail 'Directory owner/permissions differ.'
   else
      powershell.exe -NoProfile -NonInteractive -File "$(cygpath -w "$b78Directory/private-directory.ps1")" \
         -Path "$(cygpath -w "$path")" -Operation "$operation" -Anchor "$(cygpath -w "$backupDirectory")"
   fi
}
filesPrivate78() {
   local directory=$1
   (
      shopt -s dotglob nullglob
      for file in "$directory"/*; do
         [[ -f "$file" && ! -L "$file" ]] || fail 'Unexpected directory/linked/nonregular snapshot artifact.'
         if [[ "$platform" == linux ]]; then
            [[ "$(stat -c '%u:%a' -- "$file")" == "$EUID:600" ]] || fail 'Snapshot file permissions differ.'
         fi
      done
   )
   if [[ "$platform" == windows ]]; then
      TASKCOMPASS_PRIVATE_DIRECTORY=$(cygpath -w "$directory") powershell.exe -NoProfile -NonInteractive -Command '
         $ErrorActionPreference="Stop"
         $sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User
         foreach ($file in Get-ChildItem -Force -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY) {
            if ($file.PSIsContainer -or ($file.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) { throw "Nonregular snapshot file" }
            $acl=Get-Acl -LiteralPath $file.FullName
            $rules=$acl.GetAccessRules($true,$true,[System.Security.Principal.SecurityIdentifier])
            if ($acl.GetOwner([System.Security.Principal.SecurityIdentifier]).Value -ne $sid.Value -or $rules.Count -ne 1 -or
                $rules[0].IdentityReference.Value -ne $sid.Value -or $rules[0].FileSystemRights -ne [System.Security.AccessControl.FileSystemRights]::FullControl -or
                $rules[0].AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) { throw "Snapshot file ACL differs" }
         }
      '
   fi
}
sealCheck78() { (backupDirectory=$1; checkManifest "$2" "$3"); }
[[ "$qa78" == "$backupDirectory"/b77-* && "$qa78" != *'/../'* ]] || fail 'QA evidence outside approved prior capture.'
private78 "$qa78" history
sealCheck78 "$qa78" initial.sha256 'prefix.txt receipt.txt data.hex.tsv health.tsv inventory.tsv protected-inventory.tsv qa-names.tsv'
sameFiles "$qa78/data.hex.tsv" "$backupDirectory/b75-final.hex.tsv"
[[ "$(cat "$qa78/receipt.txt")" == "$receipt" ]] || fail 'B7.7 receipt differs.'
[[ "$(cat "$qa78/prefix.txt")" == "B77-QA-${qa78##*/b77-}" ]] || fail 'B7.7 prefix differs.'
if [[ "$platform" == windows ]]; then
   for label in hierarchy ownership-moves before-restart after-restart final; do
      private78 "$qa78/$label" history
      sealCheck78 "$qa78/$label" checkpoint.sha256 'data.hex.tsv health.tsv inventory.tsv'
   done
   sameFiles "$qa78/before-restart/data.hex.tsv" "$qa78/after-restart/data.hex.tsv"
else
   private78 "$qa78/final" history
   [[ -f "$qa78/final/data.hex.tsv" && ! -L "$qa78/final" && ! -L "$qa78/final/data.hex.tsv" ]] || fail 'Known failed Linux capture missing/linked; explicit review required.'
   printf 'NOTICE: Linux B7.7 protected-task modification and missing restart comparison remain unresolved.\n'
fi
packageDigest=$(sha256sum < scripts/phase12/b78/ReviewChecksums.sha256)
receipt="$receipt B7.8 ${packageDigest%% *}"
inspect78() {
   "$python78" "$b78Directory/inspect-snapshot.py" --data "$1" --dataset "$expectedDataset" \
      --baseline "$backupDirectory/b75-final.hex.tsv" --platform "$platform" "${@:2}"
}
prior78() {
   printf 'Historical references only; no rebaseline or exception waiver. Platform: %s\n' "$platform"
   [[ "$platform" != linux ]] || printf 'Linux final capture is UNSEALED; preservation/restart exceptions UNRESOLVED.\n'
   for file in "$backupDirectory/b75-final.hex.tsv" "$backupDirectory/b75-final.sha256" "$qa78/initial.sha256" "$qa78/final/data.hex.tsv"; do
      printf '%s: ' "${file##*/}"; sha256sum < "$file"
   done
}
fresh78() {
   verify75 2 taskcompass_migrate
   verify75 2 taskcompass_app
   authenticate taskcompass_dev
   [[ -n "$adminPid" ]] || adminStart
   preflight75 final 2
   query75 taskcompass_dev < scripts/phase12/b7/schema2-health.sql |
      "$python78" "$b78Directory/inspect-snapshot.py" --dataset "$expectedDataset" --health-stdin
}
files78='dump-exit.txt environment-origin.txt prior-evidence.txt attempt.txt schema2.sql source2-before.hex.tsv source2-after.hex.tsv source2-before.ddl.txt source2-after.ddl.txt health-before.tsv health-after.tsv grants-before.tsv grants-after.tsv preservation.txt dump-validation.txt environment.json client-version.txt dump-version.txt capture-complete.txt'
if [[ "$stage" == review ]]; then
   [[ -n "$destination" && "$destination" != *'/../'* ]] || fail 'Actual fresh snapshot directory required.'
   private78 "$destination"
   filesPrivate78 "$destination"
   actual=$(cd -- "$destination" && pwd -P)
   [[ "$actual/" != "$repositoryDirectory/"* && "${actual##*/}" == Phase1.2-schema2-"$platform"-* ]] || fail 'Snapshot must be outside repository with correct platform name.'
   [[ ! -e "$destination/schema2-evidence.sha256" && ! -L "$destination/schema2-evidence.sha256" && ! -e "$destination/private-review.txt" && ! -L "$destination/private-review.txt" ]] || fail 'Prior or partial review exists; no overwrite/replay.'
   [[ -f "$destination/environment-origin.txt" && ! -L "$destination/environment-origin.txt" ]] || fail 'Environment origin missing/linked.'
   reportName=$(cat "$destination/environment-origin.txt")
   [[ "$reportName" =~ ^environment-[a-zA-Z0-9._-]+\.json$ ]] || fail 'Unsafe environment sidecar name.'
   files78+=" $reportName $reportName.sha256"
   sealCheck78 "$destination" capture.sha256 "$files78"
   sealCheck78 "$destination" "$reportName.sha256" "$reportName"
   sameFiles "$destination/environment.json" "$destination/$reportName"
   sameFiles "$qa78/final/data.hex.tsv" "$destination/source2-before.hex.tsv"
   actualPrior=$(prior78 | sha256sum); expectedPrior=$(sha256sum < "$destination/prior-evidence.txt")
   [[ "${actualPrior%% *}" == "${expectedPrior%% *}" ]] || fail 'Historical reference evidence changed since capture.'
   (backupDirectory=$destination; checkHealth75 health-before.tsv; checkHealth75 health-after.tsv)
   bash "$b75Directory/compare-schema2-ddl.sh" "$backupDirectory/b75-schema2.ddl.txt" "$destination/source2-before.ddl.txt"
   [[ "$(cat "$destination/dump-exit.txt")" == 0 ]] || fail 'Dumper did not complete successfully.'
   [[ "$(cat "$destination/capture-complete.txt")" == "$receipt" ]] || fail 'Snapshot receipt differs.'
   sameFiles "$destination/source2-before.hex.tsv" "$destination/source2-after.hex.tsv"
   sameFiles "$destination/source2-before.ddl.txt" "$destination/source2-after.ddl.txt"
   inspect78 "$destination/source2-before.hex.tsv" --dump "$destination/schema2.sql"
   "$dumpReviewed" || fail 'Privately inspect every SQL statement first, then supply --dump-reviewed; no typed confirmation.'
   (cd -- "$destination"; set -o noclobber; printf 'Owner explicitly attests private SQL review via --dump-reviewed.\nLinux exceptions remain unresolved where applicable.\n' > private-review.txt
    sha256sum $files78 capture.sha256 private-review.txt > schema2-evidence.sha256
    sha256sum --check schema2-evidence.sha256)
   printf 'PASS: owner-reviewed B7.8 schema2 snapshot sealed; no restore or cleanup authorized.\nDirectory: %s\n' "$destination"; exit 0
fi
inspect78 "$qa78/final/data.hex.tsv"
dump78=${dump78:-/usr/bin/mariadb-dump}
[[ "$platform" != windows || "$dump78" != /usr/bin/mariadb-dump ]] || dump78='/c/Program Files/MariaDB 13.0/bin/mariadb-dump.exe'
[[ -x "$dump78" ]] || fail 'Required dump executable missing.'
actualDumpVersion=$(env -u MYSQL_PWD "$dump78" --no-defaults --version)
expectedDumpVersion=$(cat "$backupDirectory/dump-version.txt")
[[ "${actualDumpVersion//$'\r'/}" == "${expectedDumpVersion//$'\r'/}" ]] || fail 'Dumper version differs from reviewed B7.2 tooling; stop for review.'
fresh78
if [[ "$stage" == preflight ]]; then
   # Use stdout pipes only; no exports/private snapshots are created by preflight.
   digest=$(query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql | sha256sum)
   retained=$(sha256sum < "$qa78/final/data.hex.tsv")
   [[ "${digest%% *}" == "${retained%% *}" ]] || fail 'Current full data differs from retained B7.7 final capture; stop for review.'
   adminEnd
   TASKCOMPASS_REPORT_PYTHON="$python78" bash scripts/environment-report.sh --build-dir "$buildDirectory" --context B7.8-preflight \
      --server-evidence "$backupDirectory/b75-final-grants.txt"
   printf 'PASS: B7.8 current dev identity/schema2/final grants/health/writer checks and retained data evidence.\n'; exit 0
fi
# Capture repeats preflight immediately; no one-shot writer operation may be retried.
digest=$(query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql | sha256sum)
retained=$(sha256sum < "$qa78/final/data.hex.tsv")
[[ "${digest%% *}" == "${retained%% *}" ]] || fail 'Current data differs from B7.7; no capture destination created.'
if [[ -z "$parent78" ]]; then
   if [[ "$platform" == linux ]]; then parent78="$HOME/temp/TaskCompassBackups"
   else parent78="$HOME/TaskCompassBackups"; fi
fi
# Resolve physical ancestors before creating anything, including Windows junctions.
"$python78" - "$parent78" "$repositoryDirectory" <<'PYTHON'
from pathlib import Path
import sys
parent, repository = [Path(value).resolve() for value in sys.argv[1:]]
try:
   parent.relative_to(repository)
except ValueError:
   pass
else:
   raise SystemExit('STOP: backup parent must be outside repository')
seals = ('initial.sha256', 'capture.sha256', 'schema2-evidence.sha256', 'b7-schema1-evidence.sha256',
         'after-gui.sql.sha256', 'before-migration.sql.sha256')
for ancestor in (parent, *parent.parents):
   if any((ancestor / name).exists() for name in seals):
      raise SystemExit('STOP: do not create snapshots inside historical capture/evidence directories')
PYTHON
mkdir -p -- "$parent78"
parent78=$(cd -- "$parent78" && pwd -P)
[[ "$parent78/" != "$repositoryDirectory/"* ]] || fail 'Backup parent must be outside repository.'
destination="$parent78/Phase1.2-schema2-$platform-$(date +%Y%m%d-%H%M%S)-$$"
printf 'New attempt directory (privacy verification pending): %s\n' "$destination"
mkdir -- "$destination"; private78 "$destination" create
set -o noclobber
printf 'Fresh snapshot directory: %s\n' "$destination"
(set -o noclobber; printf '%s\n' "$receipt" > "$destination/attempt.txt")
prior78 > "$destination/prior-evidence.txt"
export78() { (set -o noclobber; query75 taskcompass_dev < "$1" > "$destination/$2"); }
export78 scripts/phase12/b7/schema2-export.sql source2-before.hex.tsv
sameFiles "$qa78/final/data.hex.tsv" "$destination/source2-before.hex.tsv"
export78 scripts/phase12/b7/ddl2-export.sql source2-before.ddl.txt
bash "$b75Directory/compare-schema2-ddl.sh" "$backupDirectory/b75-schema2.ddl.txt" "$destination/source2-before.ddl.txt"
export78 scripts/phase12/b7/schema2-health.sql health-before.tsv
(backupDirectory=$destination; checkHealth75 health-before.tsv)
printf '%s\n' "$devMetadata" > "$destination/grants-before.tsv"
inspect78 "$destination/source2-before.hex.tsv" > "$destination/preservation.txt"
dump78=${dump78:-/usr/bin/mariadb-dump}
[[ "$platform" != windows || "$dump78" != /usr/bin/mariadb-dump ]] || dump78='/c/Program Files/MariaDB 13.0/bin/mariadb-dump.exe'
[[ -x "$dump78" && ! -e "$destination/schema2.sql" && ! -L "$destination/schema2.sql" ]] || fail 'Dumper unavailable or dump path exists.'
env -u MYSQL_PWD "$clientPath" --no-defaults --version > "$destination/client-version.txt"
env -u MYSQL_PWD "$dump78" --no-defaults --version > "$destination/dump-version.txt"
dumpFile="$destination/schema2.sql"; [[ "$platform" != windows ]] || dumpFile=$(cygpath -m "$dumpFile")
authenticate taskcompass_dev
dumpStatus=0
MYSQL_PWD="${passwords[taskcompass_dev]}" "$dump78" --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
   --user=taskcompass_dev --default-character-set=utf8mb4 --single-transaction --skip-lock-tables --no-tablespaces \
   --skip-triggers --skip-add-drop-table --skip-add-locks --skip-disable-keys --no-create-db --hex-blob \
   --result-file="$dumpFile" taskcompass_dev 2> >(redact) || dumpStatus=$?
printf '%s\n' "$dumpStatus" > "$destination/dump-exit.txt"
[[ "$dumpStatus" == 0 ]] || fail 'Dump failed; preserve partial snapshot and attempt marker.'
[[ -s "$destination/schema2.sql" ]] || fail 'Dump is empty.'
export78 scripts/phase12/b7/schema2-export.sql source2-after.hex.tsv
export78 scripts/phase12/b7/ddl2-export.sql source2-after.ddl.txt
export78 scripts/phase12/b7/schema2-health.sql health-after.tsv
(backupDirectory=$destination; checkHealth75 health-after.tsv)
sameFiles "$destination/source2-before.hex.tsv" "$destination/source2-after.hex.tsv"
sameFiles "$destination/source2-before.ddl.txt" "$destination/source2-after.ddl.txt"
fresh78
printf '%s\n' "$devMetadata" > "$destination/grants-after.tsv"
finalData=$(query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql | sha256sum)
expectedData=$(sha256sum < "$destination/source2-after.hex.tsv")
[[ "${finalData%% *}" == "${expectedData%% *}" ]] || fail 'Source changed during final verification; preserve capture.'
finalDdl=$(query75 taskcompass_dev < scripts/phase12/b7/ddl2-export.sql | sha256sum)
finalDdl=${finalDdl%% *}
expectedDdl=$(sha256sum < "$destination/source2-after.ddl.txt")
[[ "$finalDdl" == "${expectedDdl%% *}" ]] || fail 'DDL changed during final verification.' 
sameFiles "$destination/grants-before.tsv" "$destination/grants-after.tsv"
adminEnd
inspect78 "$destination/source2-after.hex.tsv" --dump "$destination/schema2.sql" > "$destination/dump-validation.txt"
# Save the existing fresh server metadata through the shared offline environment reporter.
TASKCOMPASS_REPORT_PYTHON="$python78" bash scripts/environment-report.sh --build-dir "$buildDirectory" --context B7.8-capture \
   --server-evidence "$destination/grants-after.tsv" --snapshot-dir "$destination"
reportFile=''
for file in "$destination"/environment-*.json; do [[ -z "$reportFile" ]] || fail 'Unexpected duplicate environment report.'; reportFile=$file; done
cat -- "$reportFile" > "$destination/environment.json"
reportName=${reportFile##*/}
printf '%s\n' "$reportName" > "$destination/environment-origin.txt"
files78+=" $reportName $reportName.sha256"
sealCheck78 "$destination" "$reportName.sha256" "$reportName"
# Original timestamped report and checksum remain untouched and join both manifests.
filesPrivate78 "$destination"
printf '%s\n' "$receipt" > "$destination/capture-complete.txt"
(cd -- "$destination"; set -o noclobber; sha256sum $files78 > capture.sha256; sha256sum --check capture.sha256)
printf 'PASS: complete nine-table capture/data+DDL equality/health/dump contents; private SQL review PENDING.\nDirectory: %s\nPrivately inspect schema2.sql, then run 04-review with --dump-reviewed.\n' "$destination"
