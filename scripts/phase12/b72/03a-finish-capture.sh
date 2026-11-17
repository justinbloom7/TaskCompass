#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Explicit failure-review checkpoint only; never invoked automatically.
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
exec bash "$scriptDirectory/run.sh" finish "$@"
