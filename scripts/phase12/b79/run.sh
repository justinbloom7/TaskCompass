#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set +x
set -euo pipefail
umask 077
stage=${1:-help}; [[ $# == 0 ]] || shift
if [[ "$stage" == help || " $* " == *' --help '* || " $* " == *' --print-plan '* ]]; then
   printf 'B7.9 stages01 preflight,02 provision,03 restore,04 verify,05 readonly. Each requires --approved --platform linux|windows. No automatic retry/cleanup.\n'
   exit 0
fi
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
platform=''
for ((i=1; i<=$#; i++)); do
   if [[ "${!i}" == --platform ]]; then j=$((i+1)); platform=${!j:-}; fi
done
case "$platform:${MSYSTEM:-}" in linux:|windows:UCRT64) ;; *) printf 'STOP: explicit platform must match Linux Bash or Windows UCRT64.\n' >&2; exit 1 ;; esac
pythonPath=${TASKCOMPASS_REPORT_PYTHON:-$(command -v python3 || command -v python)}
if [[ "$platform" == windows ]]; then
   export MSYS2_ENV_CONV_EXCL="${MSYS2_ENV_CONV_EXCL:+$MSYS2_ENV_CONV_EXCL;}MYSQL_PWD"
   scriptPath=$(cygpath -m "$scriptDirectory/restore.py")
else scriptPath="$scriptDirectory/restore.py"; fi
exec "$pythonPath" "$scriptPath" "$stage" "$@"
