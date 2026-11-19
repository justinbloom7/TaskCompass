#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set +x
set -euo pipefail
reportPython=${TASKCOMPASS_REPORT_PYTHON:-$(command -v python3 || command -v python || true)}
[[ -n "$reportPython" ]] || { printf 'Python3 is required for the environment report; no installation attempted.\n' >&2; exit 1; }
exec "$reportPython" "$(dirname -- "${BASH_SOURCE[0]}")/environment-report.py" "$@"
