#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Accepted Phase 1.1 entry point; fixed target and behavior preserved.
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
exec bash "$scriptDirectory/../../scripts/provision-integration.sh" phase11 "$@"
