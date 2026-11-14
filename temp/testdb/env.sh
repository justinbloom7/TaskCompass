#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source this file. No credentials/files/shell profiles are written.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
   printf 'Use: source temp/testdb/env.sh [--password|--no-password]\n' >&2
   exit 2
fi
if [[ $- == *x* ]]; then
   printf 'Disable shell tracing (set +x) before loading test credentials.\n' >&2
   return 2
fi

taskCompassTestEnvironment() {
   if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --password && "$1" != --no-password ) ]]; then
      printf 'Use: source temp/testdb/env.sh [--password|--no-password]\n' >&2
      return 2
   fi
   if [[ -n "${MSYSTEM:-}" && "$MSYSTEM" != UCRT64 ]]; then
      printf 'Use the UCRT64 terminal on Windows.\n' >&2
      return 2
   fi
   if [[ "${1:-}" == --password ]]; then
      unset TASKCOMPASS_TEST_DB_PASSWORD
      if ! IFS= read -r -s -p 'Integration database password: ' TASKCOMPASS_TEST_DB_PASSWORD; then
         unset TASKCOMPASS_TEST_DB_PASSWORD
         return 1
      fi
      printf '\n' >&2
      if [[ -z "$TASKCOMPASS_TEST_DB_PASSWORD" ]]; then
         unset TASKCOMPASS_TEST_DB_PASSWORD
         printf 'An integration password is required.\n' >&2
         return 1
      fi
      export TASKCOMPASS_TEST_DB_PASSWORD
   fi
   export TASKCOMPASS_TEST_DB_HOST=127.0.0.1
   export TASKCOMPASS_TEST_DB_PORT=3306
   export TASKCOMPASS_TEST_DB_NAME=taskcompass_integration
   export TASKCOMPASS_TEST_DB_USER=taskcompass_test
   export TASKCOMPASS_TEST_DB_ISOLATED=1
}

clearTaskCompassTestPassword() {
   unset TASKCOMPASS_TEST_DB_PASSWORD
}

taskCompassTestEnvironment "$@"
