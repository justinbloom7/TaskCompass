#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Operator-only B7.7: readonly evidence or deliberate runtime GUI launch; no SQL mutations.
set +x
set -Eeuo pipefail
qaAction=${1:-help}
[[ $# == 0 ]] || shift
qaDirectory=''; qaLabel=''; compareLabel=''; pythonPath=''
qaOptions=()
usage77() {
   printf 'B7.7: 02-preflight, 03-launch, 04-checkpoint; each --approved --platform linux|windows.\n'
   printf 'Launch/checkpoint require --qa-dir PATH. Checkpoint requires fresh --label NAME; optional --compare NAME means no intervening writes expected.\n'
}
case "$qaAction" in preflight|launch|checkpoint) ;; help) usage77; exit 0 ;; *) exit 1 ;; esac
while [[ $# -gt 0 ]]; do
   case "$1" in
      --help|-h|--print-plan) usage77; exit 0 ;;
      --qa-dir|--label|--compare|--python)
         [[ $# -ge 2 && -n "$2" ]] || exit 1
         case "$1" in --qa-dir) qaDirectory=$2 ;; --label) qaLabel=$2 ;; --compare) compareLabel=$2 ;; --python) pythonPath=$2 ;; esac
         shift 2 ;;
      *) qaOptions+=("$1"); shift ;;
   esac
done
qaScriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
TASKCOMPASS_B75_LIBRARY=1 source "$qaScriptDirectory/../b75/run.sh" preflight "${qaOptions[@]}"
trap 'fail "B7.7 operator stage failed (exit $?)."' ERR
pythonPath=${pythonPath:-$(command -v python3 || command -v python || true)}
[[ -n "$pythonPath" && -x "$pythonPath" ]] || fail 'Python3 is required for offline evidence validation; no installation attempted.'
checkManifest b75-grants.sha256 'b75-grants-attempt.txt b75-grants-success.txt b75-baseline.hex.tsv b75-before-grants.txt b75-after-grants.txt'
checkManifest b75-preservation.sha256 'b75-migration-attempt.txt b75-migration-success.txt b75-legacy.hex.tsv b75-schema2.ddl.txt b75-upgrade-checks.tsv b75-health.tsv b75-schema2.hex.tsv'
checkManifest b75-runtime.sha256 'b75-runtime-attempt.txt b75-runtime-success.txt b75-final-grants.txt'
checkManifest b75-final.sha256 'b75-verification-success.txt b75-final-health.tsv b75-final.hex.tsv'
for name in b75-grants-attempt.txt b75-grants-success.txt b75-migration-attempt.txt b75-migration-success.txt b75-runtime-attempt.txt b75-runtime-success.txt b75-verification-success.txt; do marker "$name"; done
sameFiles "$backupDirectory/b75-schema2.hex.tsv" "$backupDirectory/b75-final.hex.tsv"
health77() {
   local file=$1
   checkHealth75 "${file#"$backupDirectory/"}"
}
private77() {
   [[ "$qaDirectory" == "$backupDirectory"/b77-* && "$qaDirectory" != *'/../'* && -d "$qaDirectory" && ! -L "$qaDirectory" ]] || fail 'Expected private B7.7 run directory inside approved capture.'
   local base=${qaDirectory##*/}
   [[ "$base" != *[!a-zA-Z0-9._-]* ]] || fail 'Unsafe QA directory name.'
   if [[ "$platform" == linux ]]; then
      [[ "$(stat -c '%u:%a' -- "$qaDirectory")" == "$EUID:700" ]] || fail 'QA directory is not private.'
   else
      TASKCOMPASS_PRIVATE_DIRECTORY=$(cygpath -w "$qaDirectory") powershell.exe -NoProfile -NonInteractive -Command '
         $ErrorActionPreference="Stop"
         $sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User
         $rules=(Get-Acl -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY).GetAccessRules($true,$true,[System.Security.Principal.SecurityIdentifier])
         if ($rules.Count -ne 1 -or $rules[0].IdentityReference.Value -ne $sid.Value -or
             $rules[0].FileSystemRights -ne [System.Security.AccessControl.FileSystemRights]::FullControl -or
             $rules[0].AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) { throw "Private QA ACL differs" }
      '
   fi
}
validate77() {
   local -a extra=()
   [[ $# != 3 ]] || extra=(--protected-inventory "$3")
   "$pythonPath" "$qaScriptDirectory/validate-evidence.py" --baseline "$backupDirectory/b75-final.hex.tsv" \
      --current "$1" --prefix "$qaPrefix" --dataset "$expectedDataset" \
      --manifest "$repositoryDirectory/migrations/mariadb/SchemaManifest.json" --inventory "$2" "${extra[@]}"
}
manifest77() {
   local directory=$1 manifest=$2 expected=$3 line name names=''
   [[ -f "$directory/$manifest" && ! -L "$directory/$manifest" ]] || fail 'Missing/linked QA seal.'
   while IFS= read -r line || [[ -n "$line" ]]; do
      line=${line%$'\r'}
      [[ "$line" =~ ^[0-9a-fA-F]{64}\ [\ \*](.+)$ ]] || fail 'Malformed QA checksum.'
      name=${BASH_REMATCH[1]}
      [[ " $expected " == *" $name "* && " $names " != *" $name "* && "$name" != *[!a-zA-Z0-9._-]* && -f "$directory/$name" && ! -L "$directory/$name" ]] || fail 'Unsafe/duplicate QA seal path.'
      names+="$name "
   done < "$directory/$manifest"
   [[ "$(printf '%s\n' $names | LC_ALL=C sort)" == "$(printf '%s\n' $expected | LC_ALL=C sort)" ]] || fail 'Incomplete QA manifest.'
   (cd -- "$directory" && sha256sum --check "$manifest")
}
environment77() {
   local context=$1 evidence="$backupDirectory/b75-final-grants.txt"
   if [[ -n "${devMetadata:-}" ]]; then
      evidence="$qaDirectory/server-$(date +%Y%m%d-%H%M%S)-$$.tsv"
      (set -o noclobber
       while IFS= read -r line; do
          [[ "$line" != SERVER$'\t'* ]] || printf '%s\n' "$line"
       done <<< "$devMetadata" > "$evidence")
   fi
   TASKCOMPASS_REPORT_PYTHON="$pythonPath" bash "$repositoryDirectory/scripts/environment-report.sh" \
      --build-dir "$buildDirectory" --context "B7.7-$context" --server-evidence "$evidence" \
      --snapshot-dir "$qaDirectory"
}
if [[ "$qaAction" == preflight ]]; then
   [[ -z "$qaDirectory" && -z "$qaLabel" && -z "$compareLabel" ]] || fail 'Preflight always creates a fresh directory; no override labels.'
   baseline75 2; full75
   verify75 2 taskcompass_app
   adminStart; preflight75 final 2; adminEnd
   qaDirectory="$backupDirectory/b77-$platform-$(date +%Y%m%d-%H%M%S)-$$"
   mkdir -- "$qaDirectory"; private77
   qaPrefix="B77-QA-${qaDirectory##*/b77-}"
   printf '%s\n' "$qaPrefix" > "$qaDirectory/prefix.txt"
   printf '%s\n' "$receipt" > "$qaDirectory/receipt.txt"
   printf 'role\ttitle\n' > "$qaDirectory/qa-names.tsv"
   for role in 'Root A' 'Root B' 'Sub 1' 'Sub 2' 'Project' 'Subproject' 'Project outcome' 'Pillar outcome' 'Subpillar outcome' 'Independent task' 'Direct task' 'Objective task' 'Duplicate' 'History root' 'Missing parent' 'Stale task'; do
      printf '%s\t%s %s\n' "$role" "$qaPrefix" "$role" >> "$qaDirectory/qa-names.tsv"
   done
   query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql > "$qaDirectory/data.hex.tsv"
   sameFiles "$backupDirectory/b75-final.hex.tsv" "$qaDirectory/data.hex.tsv"
   query75 taskcompass_dev < scripts/phase12/b7/schema2-health.sql > "$qaDirectory/health.tsv"
   health77 "$qaDirectory/health.tsv"
   validate77 "$qaDirectory/data.hex.tsv" "$qaDirectory/inventory.tsv" "$qaDirectory/protected-inventory.tsv"
   (cd -- "$qaDirectory" && sha256sum prefix.txt receipt.txt data.hex.tsv health.tsv inventory.tsv protected-inventory.tsv qa-names.tsv > initial.sha256)
   environment77 preflight
   printf 'PASS: B7.7 pristine B7.5 baseline/final grants/health. No GUI writes.\nQA directory: %s\nUse this exact QA prefix for EVERY new task/organization title: %s\n' "$qaDirectory" "$qaPrefix"
   exit 0
fi
private77
manifest77 "$qaDirectory" initial.sha256 'prefix.txt receipt.txt data.hex.tsv health.tsv inventory.tsv protected-inventory.tsv qa-names.tsv'
[[ "$(cat "$qaDirectory/receipt.txt")" == "$receipt" ]] || fail 'Wrong QA platform/dataset/source receipt.'
sameFiles "$backupDirectory/b75-final.hex.tsv" "$qaDirectory/data.hex.tsv"
for previous in "$qaDirectory"/*; do
   [[ ! -L "$previous" ]] || fail 'Linked QA evidence requires review.'
   if [[ -d "$previous" ]]; then
      manifest77 "$previous" checkpoint.sha256 'data.hex.tsv health.tsv inventory.tsv'
   fi
done
qaPrefix=$(cat "$qaDirectory/prefix.txt")
[[ "$qaPrefix" == "B77-QA-$platform-"* && "$qaPrefix" != *$'\n'* && "$qaPrefix" != *$'\t'* ]] || fail 'Unexpected QA prefix.'
if [[ "$qaAction" == launch ]]; then
   [[ -z "$qaLabel" && -z "$compareLabel" ]] || fail 'Launch does not accept checkpoint labels.'
   verify75 2 taskcompass_app
   gui="$buildDirectory/TaskCompass"; [[ ! -f "$gui.exe" ]] || gui+=.exe
   [[ -x "$gui" ]] || fail 'Persistent TaskCompass executable missing; build first.'
   environment77 launch
   printf 'Launching runtime GUI. Edit ONLY QA records with prefix: %s\n' "$qaPrefix"
   printf 'No SQL helper writes are performed. Restart uses this same launch command.\n'
   if TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306 TASKCOMPASS_DB_NAME=taskcompass_dev \
      TASKCOMPASS_DB_USER=taskcompass_app TASKCOMPASS_DB_ISOLATED=1 TASKCOMPASS_DB_PASSWORD="${passwords[taskcompass_app]}" "$gui"; then
      printf 'GUI exited0; manual acceptance is not inferred.\n'
   else fail 'GUI exited with an error; preserve QA records and report.'; fi
   exit 0
fi
[[ -n "$qaLabel" && "$qaLabel" != *[!a-zA-Z0-9_-]* && ${#qaLabel} -le 40 ]] || fail 'Use a fresh checkpoint --label of 1–40 letters/digits/underscore/hyphen.'
[[ -z "$compareLabel" || ( "$compareLabel" != *[!a-zA-Z0-9_-]* && ${#compareLabel} -le 40 ) ]] || fail 'Unsafe comparison label.'
checkpoint="$qaDirectory/$qaLabel"
[[ ! -e "$checkpoint" && ! -L "$checkpoint" ]] || fail 'Checkpoint exists; never overwrite or retry in place.'
# Both QA GUI sessions must be closed before checkpoint session validation.
verify75 2
adminStart; preflight75 final 2; adminEnd
authenticate taskcompass_dev
mkdir -- "$checkpoint"
query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql > "$checkpoint/data.hex.tsv"
query75 taskcompass_dev < scripts/phase12/b7/schema2-health.sql > "$checkpoint/health.tsv"
health77 "$checkpoint/health.tsv"
query75 taskcompass_dev < scripts/phase12/b7/ddl2-export.sql | bash "$b75Directory/compare-schema2-ddl.sh" "$backupDirectory/b75-schema2.ddl.txt" -
validate77 "$checkpoint/data.hex.tsv" "$checkpoint/inventory.tsv"
current=$(query75 taskcompass_dev < scripts/phase12/b7/schema2-export.sql | sha256sum)
expected=$(sha256sum < "$checkpoint/data.hex.tsv")
[[ "${current%% *}" == "${expected%% *}" ]] || fail 'Data changed during checkpoint; preserve and review.'
if [[ -n "$compareLabel" ]]; then
   [[ -d "$qaDirectory/$compareLabel" && ! -L "$qaDirectory/$compareLabel" ]] || fail 'Prior comparison checkpoint missing.'
   manifest77 "$qaDirectory/$compareLabel" checkpoint.sha256 'data.hex.tsv health.tsv inventory.tsv'
   sameFiles "$qaDirectory/$compareLabel/data.hex.tsv" "$checkpoint/data.hex.tsv"
fi
(cd -- "$checkpoint" && sha256sum data.hex.tsv health.tsv inventory.tsv > checkpoint.sha256)
environment77 "checkpoint-$qaLabel"
printf 'PASS: protected B7.5 rows/QA invariants/final grants/health; checkpoint %s sealed.\nPrivate UUID/ownership/archive inventory: %s/inventory.tsv\nManual GUI observation remains required; no cleanup or milestone acceptance inferred.\n' "$qaLabel" "$checkpoint"
