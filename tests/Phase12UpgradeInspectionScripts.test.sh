#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
TASKCOMPASS_B75_INSPECTION_FIXTURES_ONLY=1 source "$repositoryDirectory/tests/Phase12DevelopmentUpgradeScripts.test.sh"
setup75 'post-apply inspection'
result=$(passwords75 | run75 02-migration-grants --approved 2>&1)
export DEV_DDL_DRIFT=1
if result=$(passwords75 | run75 03-migrate --approved 2>&1); then exit 1; fi
[[ "$result" == *'ddl2-export.sql versus rehearsal2.ddl.txt'* && "$(cat "$DEV_SCHEMA")" == 2 && -s "$snapshot/b75-migration-attempt.txt" && ! -e "$snapshot/b75-migration-success.txt" ]]
for stage in 03-migrate 04-runtime-grants; do
 if result=$(passwords75 | run75 "$stage" --approved 2>&1); then exit 1; fi
done
mutations=$(rg -c '^dev-(grants|apply|runtime)$' "$MOCK_DIRECTORY/calls")
for mode in ddl legacy; do
 if [[ "$mode" == legacy ]]; then unset DEV_DDL_DRIFT; export DEV_LEGACY_DRIFT=1; fi
 result=$(passwords75 | run75 06-inspect-failure --approved 2>&1)
 if [[ "$mode" == ddl ]]; then [[ "$result" == *'MATCH: legacy-current.hex.tsv'* && "$result" == *'DIFFERENT: schema2-current.ddl.txt'* ]]
 else [[ "$result" == *'DIFFERENT: legacy-current.hex.tsv'* && "$result" == *'MATCH: schema2-current.ddl.txt'* ]]; fi
 [[ "$result" != *"$MOCK_SECRET"* && "$mutations" == "$(rg -c '^dev-(grants|apply|runtime)$' "$MOCK_DIRECTORY/calls")" && ! -e "$snapshot/b75-migration-success.txt" && "$(cat "$DEV_POLICY")" == migration ]]
done
unset DEV_LEGACY_DRIFT
printf 'PASS: post-apply data versus DDL failure diagnosis is read-only, sealed and never completes/replays migration or alters grants.\n'
