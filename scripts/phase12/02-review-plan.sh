#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source preview only: no client, connection, credentials or generated SQL execution.
set -euo pipefail
repositoryDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
[[ $# == 0 ]] || { printf 'Usage: bash scripts/phase12/02-review-plan.sh\n' >&2; exit 2; }
printf 'REVIEW ONLY: fixed fresh taskcompass_phase12_test on 127.0.0.1:3306.\n'
cat "$repositoryDirectory/scripts/phase12/test-account.sql"
printf '\nMigration 001 (unchanged historical source):\n'
cat "$repositoryDirectory/migrations/mariadb/001_PersistentTaskFoundation.sql"
printf '\nMigration 002 (not yet live-validated):\n'
cat "$repositoryDirectory/migrations/mariadb/002_OrganizationalManagement.sql"
sha256sum "$repositoryDirectory/migrations/mariadb/001_PersistentTaskFoundation.sql" \
   "$repositoryDirectory/migrations/mariadb/002_OrganizationalManagement.sql"
printf '\nSuite: synthetic schema-1 fixtures, upgrade/interruption cases, transactional commands and independent connections.\nOnly run-owned nine-table objects may be dropped, after empty-start proof.\nUnexpected failure stops and preserves state; no automatic recovery or retry.\nSee docs/milestones/Phase1.2.md before approving each live stage.\n'
