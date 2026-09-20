# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Copyright (C) 2026 iamankushpandit <https://github.com/iamankushpandit>
#
# Part of Ivy AI -- https://github.com/iamankushpandit/esp_ai
# Free software under GPL-3.0-or-later, with the Espressif SDK linking
# exception in LICENSE.exception. Reusing any part of this file, in any
# work, must keep this notice, credit iamankushpandit as the author,
# and stay under the same licence with corresponding source offered.
# See LICENSE, LICENSE.exception, NOTICE.md and THIRD_PARTY.md.

# Run idf.py inside the ESP-IDF v6.1 environment.
# Usage: powershell -File tools/idf.ps1 build | flash | "-p COM22 flash" ...
$ErrorActionPreference = "Continue"
. C:\esp\v6.1\esp-idf\export.ps1 *> $null
Set-Location (Split-Path $PSScriptRoot -Parent)
idf.py @args
exit $LASTEXITCODE
