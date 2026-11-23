#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Shared preflight for the numbered native Windows Stage B scripts.
set -euo pipefail

stageBRepository=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
stageBRunRoot="$stageBRepository/build-ucrt64-stage-b"

stageBFail() {
   printf '%s\n' "$*" >&2
   exit 1
}

stageBPreflight() {
   [[ $# == 0 ]] || stageBFail 'Run this script without arguments.'
   case "$(uname -s)" in
      UCRT64_NT-*|MINGW64_NT-*|MSYS_NT-*) ;;
      *) stageBFail 'Run this script on native Windows in an MSYS2 UCRT64 shell.' ;;
   esac
   [[ ${MSYSTEM:-} == UCRT64 ]] || stageBFail 'Run this script in an MSYS2 UCRT64 shell.'
   local key tool
   for key in "${!TASKCOMPASS_@}"; do unset "$key"; done
   unset MSYS2_ARG_CONV_EXCL MSYS2_ENV_CONV_EXCL MYSQL_PWD
   for tool in git cygpath cmake ninja ctest; do
      command -v "$tool" >/dev/null || stageBFail "Required tool unavailable: $tool"
   done
   stageBPython=$(command -v python3 || command -v python) || stageBFail 'Python 3 is required.'
   cd -- "$stageBRepository"
   stageBRevision=$(git rev-parse HEAD)
   [[ -z $(git status --porcelain) ]] || stageBFail 'Use a clean checkout containing the Stage B review corrections and scripts.'
   stageBSourceWindows=$(cygpath -m "$stageBRepository")
   CMAKE_PREFIX_PATH=$(cygpath -m /ucrt64)
   export CMAKE_PREFIX_PATH
}

stageBReport() {
   TASKCOMPASS_REPORT_PYTHON="$stageBPython" bash "$stageBRepository/scripts/environment-report.sh" \
      --build-dir "$stageBBuildWindows" --output-dir "$stageBEvidenceWindows" --context "$1"
}
