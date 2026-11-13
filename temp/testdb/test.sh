#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set +x
set -euo pipefail
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repositoryDirectory=$(cd -- "$scriptDirectory/../.." && pwd)
buildDirectory=''
run=false
allowReset=false
usage() {
   printf 'Usage: bash temp/testdb/test.sh --run --allow-reset [--build-dir PATH]\nNo arguments: help only. Requires approved Stage 1 live tests on an empty isolated database.\n'
}
if [[ $# -eq 0 || "${1:-}" == --help ]]; then
   usage
   exit 0
fi
while [[ $# -gt 0 ]]; do
   case "$1" in
      --run) run=true; shift ;;
      --allow-reset) allowReset=true; shift ;;
      --build-dir)
         [[ $# -ge 2 ]] || { usage >&2; exit 2; }
         buildDirectory=$2; shift 2 ;;
      *) usage >&2; exit 2 ;;
   esac
done
"$run" && "$allowReset" || { usage >&2; exit 2; }
# Clear credentials on every exit, including invalid config or missing executable.
trap 'unset TASKCOMPASS_TEST_DB_PASSWORD MYSQL_PWD' EXIT
for setting in HOST PORT NAME USER ISOLATED; do
   variableName="TASKCOMPASS_TEST_DB_$setting"
   case "$setting" in
      HOST) expected=127.0.0.1 ;;
      PORT) expected=3306 ;;
      NAME) expected=taskcompass_integration ;;
      USER) expected=taskcompass_test ;;
      ISOLATED) expected=1 ;;
   esac
   if [[ -v "$variableName" && "${!variableName}" != "$expected" ]]; then
      printf 'Conflicting %s; refusing to change the target silently.\n' "$variableName" >&2
      exit 2
   fi
done
source "$scriptDirectory/env.sh" --no-password
if [[ -z "$buildDirectory" ]]; then
   if [[ "${MSYSTEM:-}" == UCRT64 ]]; then
      buildDirectory="$repositoryDirectory/build-ucrt64"
   else
      buildDirectory="$repositoryDirectory/build"
   fi
fi
executable="$buildDirectory/MariaDbIntegrationTests"
[[ ! -f "$executable.exe" ]] || executable+=.exe
[[ -x "$executable" ]] || { printf 'Build MariaDbIntegrationTests first, or specify --build-dir.\n' >&2; exit 2; }
if [[ -z "${TASKCOMPASS_TEST_DB_PASSWORD:-}" ]]; then
   source "$scriptDirectory/env.sh" --password
fi
printf 'Running disposable integration tests on taskcompass_integration@127.0.0.1:3306.\n'
# Preserve the executable's exact status, including 1 failure and 77 skipped.
set +e
"$executable" --run --allow-reset
result=$?
set -e
unset TASKCOMPASS_TEST_DB_PASSWORD
printf 'Integration test exit code: %s\n' "$result"
exit "$result"
