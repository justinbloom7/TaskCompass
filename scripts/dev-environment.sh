#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source this file; no credentials or shell configuration files are written.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
   printf 'Use: source scripts/dev-environment.sh [--password|--no-password]\n' >&2
   exit 1
fi
if [[ $- == *x* ]]; then
   printf 'Disable shell tracing (set +x) before loading credentials.\n' >&2
   return 1
fi

taskCompassDevEnvironment() {
   if [[ $# -gt 1 || ( $# -eq 1 && "$1" != '--password' && "$1" != '--no-password' ) ]]; then
      printf 'Use: source scripts/dev-environment.sh [--password|--no-password]\n' >&2
      return 1
   fi
   if [[ -n "${MSYSTEM:-}" && "$MSYSTEM" != 'UCRT64' ]]; then
      printf 'Use the MSYS2 UCRT64 terminal for this build environment.\n' >&2
      return 1
   fi
   if [[ "${MSYSTEM:-}" == 'UCRT64' ]]; then
      export PATH="/ucrt64/bin:$PATH"
      export CMAKE_PREFIX_PATH
      CMAKE_PREFIX_PATH=$(cygpath -m /ucrt64) || return 1
   fi
   export TASKCOMPASS_DB_HOST=127.0.0.1
   export TASKCOMPASS_DB_PORT=3306
   export TASKCOMPASS_DB_NAME=taskcompass_dev
   export TASKCOMPASS_DB_USER=taskcompass_dev
   export TASKCOMPASS_DB_ISOLATED=1
   if [[ "${1:-}" == '--password' ]]; then
      unset TASKCOMPASS_DB_PASSWORD
      IFS= read -r -s -p 'TaskCompass development database password: ' TASKCOMPASS_DB_PASSWORD || return 1
      printf '\n' >&2
      export TASKCOMPASS_DB_PASSWORD
   fi
   printf 'TaskCompass environment: taskcompass_dev@127.0.0.1:3306; no files written.\n'
}

clearTaskCompassDatabasePassword() {
   unset TASKCOMPASS_DB_PASSWORD
}

taskCompassDevEnvironment "$@"
