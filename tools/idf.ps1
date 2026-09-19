# Run idf.py inside the ESP-IDF v6.1 environment.
# Usage: powershell -File tools/idf.ps1 build | flash | "-p COM22 flash" ...
$ErrorActionPreference = "Continue"
. C:\esp\v6.1\esp-idf\export.ps1 *> $null
Set-Location (Split-Path $PSScriptRoot -Parent)
idf.py @args
exit $LASTEXITCODE
