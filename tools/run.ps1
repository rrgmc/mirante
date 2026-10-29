# Starts the Mirante app built by tools/build.ps1, on Windows, from any shell.
#
#   tools/run.ps1                       the dev build, last workspace used
#   tools/run.ps1 -Data <folder>        open the workspace in <folder>
#   tools/run.ps1 -Build                build the preset first, without tests
#   tools/run.ps1 -Preset release       another preset's build
#
# Adds Qt's bin folder to PATH for the app, since the Qt DLLs are not copied
# next to it.

param(
    [string]$Preset = 'dev',
    [string]$Data,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot

if ($Build) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset -NoTest
}

$Exe = Join-Path $Repo "build\$Preset\mirante.exe"
if (-not (Test-Path $Exe)) { throw "$Exe was not found. Build it with tools/build.ps1 -Preset $Preset, or add -Build." }

$QtDir = & (Join-Path $PSScriptRoot 'setup-qt.ps1') | Select-Object -Last 1
$env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"

$Start = @{ FilePath = $Exe }
# Start-Process joins its arguments with spaces, so the folder is quoted.
if ($Data) { $Start.ArgumentList = '--data', "`"$((Resolve-Path $Data).Path)`"" }
Start-Process @Start
