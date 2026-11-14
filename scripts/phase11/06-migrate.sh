#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Stage 06 of 11; shared implementation preserves all safety gates.
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
exec bash "$scriptDirectory/../phase11-validate.sh" 06-migrate "$@"
