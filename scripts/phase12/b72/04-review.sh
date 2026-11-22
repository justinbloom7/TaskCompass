#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
scriptDirectory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || exit 1
exec bash "$scriptDirectory/run.sh" review "$@"
