#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Exact DDL comparison except the reviewed implicit LocalChange FK index alias/position.
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: compare-schema2-ddl.sh EXPECTED ACTUAL|-\n' >&2; exit 1; }
canonical() {
   local file=$1 line='' content name inside=false found=0 indexes=0 closed=0
   [[ "$file" != - ]] || file=/dev/stdin
   while IFS= read -r line; do
      content=${line%$'\r'}
      if [[ "$content" == *$'\tCREATE TABLE '* ]]; then
         "$inside" && return 1
         name=${content%%$'\t'*}
         if [[ "${name,,}" == localchange ]]; then inside=true; found=$((found+1)); fi
      fi
      if [[ "$content" == '  KEY `changeTask` '* || "$content" == '  KEY `changeTaskV2` '* ]]; then
         "$inside" || return 1
         case "$content" in
            '  KEY `changeTask` (`datasetId`,`taskId`),'|'  KEY `changeTaskV2` (`datasetId`,`taskId`),') ;;
            *) return 1 ;;
         esac
         indexes=$((indexes+1))
         continue
      fi
      if "$inside" && [[ "$content" == ') ENGINE='* ]]; then inside=false; closed=$((closed+1)); fi
      printf '%s\n' "$line"
   done < "$file"
   # Reject malformed/truncated exports and duplicate/missing supporting indexes.
   [[ -z "$line" && "$found" == 1 && "$indexes" == 1 && "$closed" == 1 ]] && ! "$inside"
}
expected=$(canonical "$1" | sha256sum) || { printf 'STOP: expected DDL supporting-index form is not recognized.\n' >&2; exit 1; }
actual=$(canonical "$2" | sha256sum) || { printf 'STOP: actual DDL supporting-index form is not recognized.\n' >&2; exit 1; }
[[ "${expected%% *}" == "${actual%% *}" ]] || { printf 'STOP: schema2 DDL differs beyond the reviewed LocalChange supporting-index alias/position.\n' >&2; exit 1; }
printf 'PASS: schema2 DDL matches; only LocalChange changeTask/changeTaskV2 identical nonunique (datasetId,taskId) index alias/position may differ.\n'
