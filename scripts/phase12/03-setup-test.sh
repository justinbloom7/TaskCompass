#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
case "${1:---help}" in
   --apply)
      [[ "${2:-}" == --approved ]] || { printf 'STOP: separate provisioning approval required; then use --apply --approved.\n' >&2; exit 2; }
      shift 2
      exec bash "$scriptDirectory/../provision-integration.sh" phase12 --apply "$@" ;;
   *) exec bash "$scriptDirectory/../provision-integration.sh" phase12 "$@" ;;
esac
