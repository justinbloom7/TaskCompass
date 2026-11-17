#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# B7.2 operator-only orchestration. Never imports, migrates, grants or repairs.
set +x
set -Eeuo pipefail
umask 077
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$scriptDirectory/../../.." && pwd)
cd -- "$repositoryDirectory"
action=${1:-help}
[[ $# == 0 ]] || shift
approved=false
clientPath=''
dumpPath=''
buildDirectory=''
backupDirectory=''
accountPassword=''
declare -A passwords=()
readonly MAX_PASSWORD_ATTEMPTS=3
trap 'unset accountPassword passwords TASKCOMPASS_DB_PASSWORD MYSQL_PWD' EXIT
fail() { printf 'STOP: %s Preserve evidence; no retry, cleanup or repair.\n' "$1" >&2; exit 1; }
trap 'fail "B7.2 operation failed (exit $?)."' ERR
usage() {
   cat <<'USAGE'
B7.2 only; run numbered scripts one at a time on Linux or MSYS2 UCRT64:
  01-check.sh                 Database-free tool/checksum checks.
  02-preflight.sh --approved Read-only baseline/grants report; manually review.
  03-capture.sh --approved   Confirm reviewed baseline, fresh verify1 and private backup.
  03a-finish-capture.sh       ONLY after separate failure-review approval; missing metadata only.
  04-review.sh               Prompt for captured directory, checksums and private SQL review.
Options: --client PATH --dump PATH --build-dir PATH --backup-dir PATH
Defaults select local platform tools; capture chooses a fresh external timestamped
folder. Review prompts for its printed path, or accepts --backup-dir.
--help / --print-plan never connects or captures. Missing --approved on 02/03
cannot connect. Owner approved B7.2 only; no migration/restore/reset/GUI/grants.
TaskCompass account entry: up to 3 authentication-only attempts; checked passwords
stay in memory for this run. Backup/export/verify operations are never retried.
USAGE
}
[[ "$action" != help ]] || { usage; exit 0; }
case "$action" in check|preflight|capture|finish|review) ;; *) fail 'Unknown stage.' ;; esac
while [[ $# -gt 0 ]]; do
   case "$1" in
      --help|-h|--print-plan) usage; exit 0 ;;
      --approved) approved=true; shift ;;
      --client|--dump|--build-dir|--backup-dir)
         [[ $# -ge 2 && -n "$2" ]] || fail "Missing $1 value."
         case "$1" in
            --client) clientPath=$2 ;; --dump) dumpPath=$2 ;;
            --build-dir) buildDirectory=$2 ;; --backup-dir) backupDirectory=$2 ;;
         esac
         shift 2 ;;
      *) fail "Unknown option $1." ;;
   esac
done
if [[ "$action" == preflight || "$action" == capture || "$action" == finish ]]; then
   "$approved" || fail 'Use --approved only under the recorded B7.2 approval.'
fi
case "$action" in check) stageNumber=01 ;; preflight) stageNumber=02 ;; capture) stageNumber=03 ;; finish) stageNumber=03a ;; review) stageNumber=04 ;; esac
printf 'Stage %s/04: B7.2 %s\n' "$stageNumber" "$action"
if [[ -n "${MSYSTEM:-}" ]]; then
   [[ "$MSYSTEM" == UCRT64 ]] || fail 'Use MSYS2 UCRT64 on Windows.'
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}MYSQL_PWD;TASKCOMPASS_DB_PASSWORD"
   clientPath=${clientPath:-'/c/Program Files/MariaDB 13.0/bin/mariadb.exe'}
   dumpPath=${dumpPath:-'/c/Program Files/MariaDB 13.0/bin/mariadb-dump.exe'}
   buildDirectory=${buildDirectory:-"$repositoryDirectory/build-ucrt64"}
   expectedDataset=457bb52b-fea4-4347-9c47-675c6b035c90
   defaultParent="$HOME/TaskCompassBackups"
   platform=windows
else
   clientPath=${clientPath:-/usr/bin/mariadb}
   dumpPath=${dumpPath:-/usr/bin/mariadb-dump}
   buildDirectory=${buildDirectory:-"$repositoryDirectory/build"}
   expectedDataset=21be9da7-b767-4578-b972-cc9a35122ddb
   defaultParent="$HOME/temp/TaskCompassBackups"
   platform=linux
fi
executable="$buildDirectory/TaskCompassMigrate"
[[ ! -f "$executable.exe" ]] || executable+=.exe
sqlDirectory="$repositoryDirectory/scripts/phase12/b7"
confirm() {
   local response=''
   IFS= read -r -p "Type $1: " response || fail 'Cancelled.'
   [[ "$response" == "$1" ]] || fail 'Confirmation mismatch.'
}
redact() {
   local line=''
   while IFS= read -r line || [[ -n "$line" ]]; do
      local secret=''
      for secret in "${accountPassword:-}" "${passwords[@]}"; do
         [[ -z "$secret" ]] || line=${line//"$secret"/<redacted>}
      done
      if [[ "$line" == 'WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.' ]]; then
         printf 'MariaDB password-option notice; TLS settings unchanged.\n' >&2
      else
         printf '%s\n' "$line" >&2
      fi
   done
}
sameFiles() {
   local firstHash secondHash
   firstHash=$(sha256sum < "$1")
   secondHash=$(sha256sum < "$2")
   [[ "${firstHash%% *}" == "${secondHash%% *}" ]] || fail 'Evidence files differ.'
}
# Retry only this read-only login/identity probe and only native 1045/28000.
# Once verified, cache in shell memory; never retry any subsequent operation.
authenticate() {
   local user=$1 attempt authOutput='' authStatus=0 line='' identity='' errors=0 unexpected=false
   case "$user" in taskcompass_migrate|taskcompass_app|taskcompass_dev) ;; *) fail 'Unsupported credential account.' ;; esac
   [[ ! -v "passwords[$user]" ]] || return 0
   local expectedIdentity=$'taskcompass_dev\t'"$user@127.0.0.1"
   for ((attempt=1; attempt<=MAX_PASSWORD_ATTEMPTS; attempt+=1)); do
      IFS= read -r -s -p "[$platform] Password for $user (attempt $attempt/$MAX_PASSWORD_ATTEMPTS; Ctrl-D cancels): " accountPassword || fail 'Password entry cancelled.'
      printf '\n' >&2
      if [[ -z "$accountPassword" ]]; then
         printf 'Empty password; enter it again.\n' >&2
         continue
      fi
      if authOutput=$(printf '%s\n' 'SELECT DATABASE(),CURRENT_USER();' |
         LC_ALL=C MYSQL_PWD="$accountPassword" "$clientPath" --no-defaults --protocol=TCP \
            --host=127.0.0.1 --port=3306 --user="$user" --connect-timeout=5 \
            --default-character-set=utf8mb4 --batch --raw --skip-column-names \
            --database=taskcompass_dev 2>&1); then authStatus=0; else authStatus=$?; fi
      identity=''; errors=0; unexpected=false
      while IFS= read -r line || [[ -n "$line" ]]; do
         line=${line%$'\r'}
         if [[ "$line" == "$expectedIdentity" && -z "$identity" ]]; then identity=$line
         elif [[ "$line" == 'WARNING: option --ssl-verify-server-cert is disabled, because of an insecure passwordless login.' ]]; then
            printf 'MariaDB password-option notice; TLS settings unchanged.\n' >&2
         elif [[ "$line" =~ ^ERROR\ 1045\ \(28000\):\ Access\ denied\ for\ user\  ]]; then errors=$((errors+1))
         else unexpected=true; fi
      done <<< "$authOutput"
      if [[ "$authStatus" == 0 && "$identity" == "$expectedIdentity" && "$errors" == 0 ]] && ! "$unexpected"; then
         passwords[$user]=$accountPassword
         unset accountPassword authOutput
         printf 'Credential checked: %s (memory only for this run).\n' "$user"
         return 0
      fi
      # No raw error text is printed: errors may contain connection/secret data.
      if [[ "$authStatus" != 0 && "$errors" == 1 && -z "$identity" ]] && ! "$unexpected"; then
         unset accountPassword authOutput
         printf 'Authentication rejected for %s; no backup/export was attempted by this probe.\n' "$user" >&2
      else
         unset accountPassword authOutput
         fail "Login/identity check for $user failed for a reason other than a confirmed password rejection; not retried."
      fi
   done
   fail "Password entry for $user exhausted $MAX_PASSWORD_ATTEMPTS attempts."
}
verifySchema1() {
   local output=''
   authenticate taskcompass_migrate
   output=$(TASKCOMPASS_DB_HOST=127.0.0.1 TASKCOMPASS_DB_PORT=3306 \
      TASKCOMPASS_DB_NAME=taskcompass_dev TASKCOMPASS_DB_USER=taskcompass_migrate \
      TASKCOMPASS_DB_ISOLATED=1 TASKCOMPASS_DB_PASSWORD="${passwords[taskcompass_migrate]}" \
      "$executable" --verify --to-version 1 2> >(redact)) || fail 'Read-only schema-1 verification failed; no operation retry.'
   output=${output//$'\r'/}
   printf '%s\n' "$output"
   [[ "$output" == "Schema 1 verified; dataset $expectedDataset" ]] || fail 'Unexpected verified schema/dataset.'
}
exportReadOnly() {
   local sqlFile=$1 outputFile=$2
   shift 2
   authenticate taskcompass_dev
   [[ ! -e "$outputFile" && ! -L "$outputFile" ]] || fail 'Export already exists.'
   (set -o noclobber
    MYSQL_PWD="${passwords[taskcompass_dev]}" "$clientPath" --no-defaults --protocol=TCP \
       --host=127.0.0.1 --port=3306 --user=taskcompass_dev \
       --default-character-set=utf8mb4 --batch --skip-column-names \
       --database=taskcompass_dev "$@" < "$sqlFile" > "$outputFile" 2> >(redact))
}
checkManifest() {
   local manifest=$1 expected=$2 line='' name='' names=''
   [[ -f "$backupDirectory/$manifest" && ! -L "$backupDirectory/$manifest" ]] || fail 'Missing/linked manifest.'
   while IFS= read -r line || [[ -n "$line" ]]; do
      line=${line%$'\r'}
      [[ "$line" =~ ^[0-9a-fA-F]{64}\ [\ \*](.+)$ ]] || fail 'Malformed checksum entry.'
      name=${BASH_REMATCH[1]}
      [[ "$name" != *[![:alnum:]._-]* && " $expected " == *" $name "* ]] || fail 'Unexpected checksum path.'
      [[ " $names " != *" $name "* ]] || fail 'Duplicate checksum entry.'
      [[ -f "$backupDirectory/$name" && ! -L "$backupDirectory/$name" ]] || fail 'Missing/linked evidence file.'
      names+="$name "
   done < "$backupDirectory/$manifest"
   [[ "$(printf '%s\n' $names | LC_ALL=C sort)" == "$(printf '%s\n' $expected | LC_ALL=C sort)" ]] || fail 'Incomplete checksum coverage.'
   (cd -- "$backupDirectory" && sha256sum --check "$manifest")
}
checkEvidence() {
   checkManifest after-gui.sql.sha256 'after-gui.sql'
   checkManifest source.tsv.sha256 'source.tsv'
   checkManifest b7-schema1-evidence.sha256 'legacy-before.hex.tsv legacy-after.hex.tsv schema1.ddl.txt source-after.tsv dataset.txt server.txt client-version.txt dump-version.txt'
   test -s "$backupDirectory/after-gui.sql"
   [[ "$(tr -d '\r\n' < "$backupDirectory/dataset.txt")" == "$expectedDataset" ]] || fail 'Dataset evidence belongs to another platform.'
   sameFiles "$backupDirectory/source.tsv" "$backupDirectory/source-after.tsv"
   sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/legacy-after.hex.tsv"
}
validateCaptureDirectory() {
   [[ -n "$backupDirectory" && -d "$backupDirectory" && ! -L "$backupDirectory" ]] || fail 'Expected existing private capture directory.'
   backupDirectory=$(cd -- "$backupDirectory" && pwd -P)
   [[ "$backupDirectory/" != "$repositoryDirectory/"* ]] || fail 'Backup must be outside repository.'
   if [[ "$platform" == linux ]]; then
      [[ "$(stat -c '%u:%a' -- "$backupDirectory")" == "$EUID:700" ]] || fail 'Capture directory is not private to this user.'
   else
      TASKCOMPASS_PRIVATE_DIRECTORY=$(cygpath -w "$backupDirectory") powershell.exe -NoProfile -NonInteractive -Command '
         $ErrorActionPreference="Stop"
         $sid=[System.Security.Principal.WindowsIdentity]::GetCurrent().User
         $acl=Get-Acl -LiteralPath $env:TASKCOMPASS_PRIVATE_DIRECTORY
         $rules=$acl.GetAccessRules($true,$true,[System.Security.Principal.SecurityIdentifier])
         if (!$acl.AreAccessRulesProtected -or $rules.Count -ne 1 -or
             $rules[0].IdentityReference.Value -ne $sid.Value -or
             $rules[0].FileSystemRights -ne [System.Security.AccessControl.FileSystemRights]::FullControl -or
             $rules[0].AccessControlType -ne [System.Security.AccessControl.AccessControlType]::Allow) {
            throw "Private capture ACL validation failed"
         }
      '
   fi
}
sealEvidence() {
   (set -o noclobber
    "$clientPath" --no-defaults --version > "$backupDirectory/client-version.txt"
    "$dumpPath" --no-defaults --version > "$backupDirectory/dump-version.txt")
   (cd -- "$backupDirectory" && set -o noclobber && sha256sum \
      legacy-before.hex.tsv legacy-after.hex.tsv schema1.ddl.txt source-after.tsv \
      dataset.txt server.txt client-version.txt dump-version.txt > b7-schema1-evidence.sha256)
   checkEvidence
}
if [[ "$action" == review ]]; then
   if [[ -z "$backupDirectory" ]]; then
      IFS= read -r -p 'Paste the backup directory printed by step 03: ' backupDirectory || fail 'Cancelled.'
   fi
   validateCaptureDirectory
   checkEvidence
   sha256sum "$backupDirectory/after-gui.sql"
   printf 'Privately inspect every SQL statement; quit less with q. No SQL is executed.\n'
   printf 'Expect only the five schema-1 tables, INSERT data and normal session settings.\n'
   printf 'Reject database/account/grant statements, DROP/REPLACE/TRUNCATE, GLOBAL changes, stored code/DEFINER or redirected writes.\n'
   LESSHISTFILE=- less -- "$backupDirectory/after-gui.sql"
   confirm 'DUMP REVIEWED'
   printf 'PASS: B7.2 checksums/comparisons and owner-confirmed private review.\nDirectory: %s\nSTOP: no restore/migration is authorized.\n' "$backupDirectory"
   exit 0
fi
sha256sum --check "$sqlDirectory/ReviewChecksums.sha256"
[[ -x "$clientPath" && -x "$dumpPath" && -x "$executable" ]] || fail 'Required native client/dump or built verifier is missing.'
if [[ "$action" == check ]]; then
   command -v less >/dev/null || fail 'less is required for private dump review.'
   if [[ "$platform" == windows ]]; then
      command -v powershell.exe >/dev/null || fail 'PowerShell is required for private ACL validation.'
      command -v cygpath >/dev/null || fail 'cygpath is required for Windows native paths.'
   else
      command -v sudo >/dev/null || fail 'sudo is required for the local administrator preflight.'
   fi
   "$clientPath" --no-defaults --version
   "$dumpPath" --no-defaults --version
   printf 'PASS: offline B7.2 tools/checksums. No connection or backup.\n'
   exit 0
fi
if [[ "$action" == preflight ]]; then
   if [[ "$platform" == linux ]]; then
      env -u MYSQL_PWD sudo -v
      cat "$sqlDirectory/preflight-dev.sql" "$sqlDirectory/preflight-dev-grants.sql" |
         env -u MYSQL_PWD sudo -n -- "$clientPath" --no-defaults --protocol=SOCKET --user=root \
            --default-character-set=utf8mb4 --batch --raw --database=taskcompass_dev
   else
      printf 'Enter the WINDOWS MariaDB root password at the native client prompt.\n'
      cat "$sqlDirectory/preflight-dev.sql" "$sqlDirectory/preflight-dev-grants.sql" "$sqlDirectory/public-grants.sql" |
         env -u MYSQL_PWD "$clientPath" --no-defaults --protocol=TCP --host=127.0.0.1 --port=3306 \
            --user=root --password --default-character-set=utf8mb4 --batch --raw --database=taskcompass_dev
   fi
   if [[ "$platform" == linux ]]; then
      printf 'Expected: feledria / 10.5.29 / Task 4, CommandRecord 28, LocalChange 28.\n'
   else
      printf 'Expected: feldria-w10 / 13.0.2 / Task 2, CommandRecord 16, LocalChange 16.\n'
   fi
   printf 'Expected dataset: %s; schema 1, five InnoDB tables, one Dataset, no other sessions/stored code.\n' "$expectedDataset"
   printf 'Expected rights: diagnostic/migrator schema SELECT; app 9 table rights; no extra roles/grants. Windows PUBLIC test scopes are unrelated.\n'
   printf 'Report executed; unchanged baseline/grants still require YOUR review against B7.1.\nIf ANY baseline/grant differs, STOP; step 03 requires your unchanged-baseline confirmation.\n'
   exit 0
fi
if [[ "$action" == finish ]]; then
   [[ -n "$backupDirectory" ]] || fail 'Completion requires the exact reviewed --backup-dir.'
   confirm 'FINISH REVIEWED CAPTURE; WRITERS CLOSED'
   validateCaptureDirectory
   for name in client-version.txt dump-version.txt b7-schema1-evidence.sha256; do
      [[ ! -e "$backupDirectory/$name" && ! -L "$backupDirectory/$name" ]] || fail 'Completion metadata already exists; review required.'
   done
   for name in legacy-before.hex.tsv legacy-after.hex.tsv schema1.ddl.txt source-after.tsv dataset.txt server.txt; do
      [[ -s "$backupDirectory/$name" && ! -L "$backupDirectory/$name" ]] || fail 'Expected completed capture file missing/linked/empty.'
   done
   checkManifest after-gui.sql.sha256 'after-gui.sql'
   test -s "$backupDirectory/after-gui.sql"
   checkManifest source.tsv.sha256 'source.tsv'
   [[ "$(tr -d '\r\n' < "$backupDirectory/dataset.txt")" == "$expectedDataset" ]] || fail 'Captured dataset differs.'
   sameFiles "$backupDirectory/source.tsv" "$backupDirectory/source-after.tsv"
   sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/legacy-after.hex.tsv"
   authenticate taskcompass_dev
   printf 'Checking current-data hash with cached diagnostic credentials (no export file).\n'
   currentHash=$(MYSQL_PWD="${passwords[taskcompass_dev]}" "$clientPath" --no-defaults --protocol=TCP \
      --host=127.0.0.1 --port=3306 --user=taskcompass_dev \
      --default-character-set=utf8mb4 --batch --skip-column-names \
      --database=taskcompass_dev < "$sqlDirectory/legacy-export.sql" 2> >(redact) | sha256sum)
   capturedHash=$(sha256sum < "$backupDirectory/legacy-before.hex.tsv")
   [[ "${currentHash%% *}" == "${capturedHash%% *}" ]] || fail 'Current data differs from reviewed capture.'
   verifySchema1
   sealEvidence
   sha256sum "$backupDirectory/after-gui.sql"
   printf 'PASS: reviewed B7.2 completion; original capture files unchanged.\nDirectory: %s\nRun 04-review.sh next; no restore/migration is authorized.\n' "$backupDirectory"
   exit 0
fi
confirm 'BASELINE UNCHANGED; WRITERS CLOSED'
backupDirectory=${backupDirectory:-"$defaultParent/Phase1.2-schema1-$platform-$(date +%Y%m%d-%H%M%S)"}
[[ ! -e "$backupDirectory" && ! -L "$backupDirectory" ]] || fail 'Destination exists; never overwrite.'
printf 'Fresh destination: %s\n' "$backupDirectory"
verifySchema1
authenticate taskcompass_app
authenticate taskcompass_dev
confirm 'ALL TASKCOMPASS WRITERS CLOSED'
printf 'Reusing checked app/diagnostic credentials for the backup helper; no password re-entry is needed.\n'
# Forward only the explicitly entered confirmation and memory-cached passwords
# over an anonymous pipe; no secrets in arguments, files, history or stdout.
builtin printf 'ALL TASKCOMPASS WRITERS CLOSED\n%s\n%s\n' "${passwords[taskcompass_app]}" "${passwords[taskcompass_dev]}" |
   bash "$repositoryDirectory/scripts/phase11/09-backup.sh" --backup-dir "$backupDirectory" \
      --client "$clientPath" --dump "$dumpPath" --build-dir "$buildDirectory"
[[ -d "$backupDirectory" && ! -L "$backupDirectory" ]] || fail 'Private helper directory missing.'
[[ "$(tr -d '\r\n' < "$backupDirectory/dataset.txt")" == "$expectedDataset" ]] || fail 'Backup helper dataset mismatch.'
test -s "$backupDirectory/after-gui.sql"
sameFiles "$backupDirectory/source.tsv" "$backupDirectory/source-after.tsv"
(cd -- "$backupDirectory" && sha256sum --check after-gui.sql.sha256 source.tsv.sha256)
exportReadOnly "$sqlDirectory/legacy-export.sql" "$backupDirectory/legacy-before.hex.tsv"
exportReadOnly "$sqlDirectory/ddl1-export.sql" "$backupDirectory/schema1.ddl.txt" --raw
exportReadOnly "$sqlDirectory/legacy-export.sql" "$backupDirectory/legacy-after.hex.tsv"
sameFiles "$backupDirectory/legacy-before.hex.tsv" "$backupDirectory/legacy-after.hex.tsv"
verifySchema1
sealEvidence
sha256sum "$backupDirectory/after-gui.sql"
printf 'PASS: B7.2 capture, comparisons and checksums. Private SQL review still pending.\nDirectory: %s\nRun 04-review.sh next; paste this directory when prompted.\n' "$backupDirectory"
