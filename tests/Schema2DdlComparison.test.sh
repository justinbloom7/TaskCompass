#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temporaryDirectory=$(mktemp -d)
trap 'rm -rf -- "$temporaryDirectory"' EXIT
comparison="$repositoryDirectory/scripts/phase12/b75/compare-schema2-ddl.sh"
printf 'LocalChange\tCREATE TABLE `LocalChange` (\n  `id` char(36),\n  KEY `changeTask` (`datasetId`,`taskId`),\n  KEY `changePending` (`datasetId`,`acknowledgmentState`,`createdAt`),\n) ENGINE=InnoDB\n' > "$temporaryDirectory/expected"
cp "$temporaryDirectory/expected" "$temporaryDirectory/actual"
bash "$comparison" "$temporaryDirectory/expected" "$temporaryDirectory/actual" >/dev/null
printf 'LocalChange\tCREATE TABLE `LocalChange` (\n  `id` char(36),\n  KEY `changePending` (`datasetId`,`acknowledgmentState`,`createdAt`),\n  KEY `changeTaskV2` (`datasetId`,`taskId`),\n) ENGINE=InnoDB\n' > "$temporaryDirectory/actual"
bash "$comparison" "$temporaryDirectory/expected" "$temporaryDirectory/actual" >/dev/null
bash "$comparison" "$temporaryDirectory/actual" "$temporaryDirectory/expected" >/dev/null
cat "$temporaryDirectory/actual" | bash "$comparison" "$temporaryDirectory/expected" - >/dev/null
sed 's/$/\r/' "$temporaryDirectory/expected" > "$temporaryDirectory/expected-crlf"
sed 's/$/\r/' "$temporaryDirectory/actual" > "$temporaryDirectory/actual-crlf"
bash "$comparison" "$temporaryDirectory/expected-crlf" "$temporaryDirectory/actual-crlf" >/dev/null
if bash "$comparison" "$temporaryDirectory/expected" "$temporaryDirectory/actual-crlf" >/dev/null 2>&1; then exit 1; fi
for substitution in 's/`taskId`/`wrongId`/' 's/KEY `changeTaskV2`/UNIQUE KEY `changeTaskV2`/' 's/`changeTaskV2`/`unknownIndex`/' 's/char(36)/char(35)/' 's/ENGINE=InnoDB/ENGINE=MyISAM/' 's/`datasetId`,`taskId`/`taskId`,`datasetId`/'; do
 sed "$substitution" "$temporaryDirectory/actual" > "$temporaryDirectory/wrong"
 if bash "$comparison" "$temporaryDirectory/expected" "$temporaryDirectory/wrong" >/dev/null 2>&1; then exit 1; fi
done
cat "$temporaryDirectory/actual" "$temporaryDirectory/actual" > "$temporaryDirectory/wrong"
if bash "$comparison" "$temporaryDirectory/expected" "$temporaryDirectory/wrong" >/dev/null 2>&1; then exit 1; fi
printf 'PASS: exact DDL and only the reviewed supporting-index alias/position accepted; column/uniqueness/name/type/engine/duplicate differences rejected.\n'
