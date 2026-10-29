# Starts the Mirante app built by tools/build.ps1, on Windows, from any shell.
#
#   tools/run.ps1                       the dev build, last workspace used
#   tools/run.ps1 -Data <folder>        open the workspace in <folder>
#   tools/run.ps1 -Settings <file>      keep the settings in an INI file, so a
#                                       test run leaves this device's alone
#   tools/run.ps1 -Log                  keep the log for Help > Show Log, also
#                                       in a release build
#   tools/run.ps1 -Build                build the preset first, without tests
#   tools/run.ps1 -Preset release       another preset's build
#
# Adds Qt's bin folder to PATH for the app, since the Qt DLLs are not copied
# next to it.

param(
    [string]$Preset = 'dev',
    [string]$Data,
    [string]$Settings,
    [switch]$Log,
    [switch]$Build
)

$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot
$Exe = Join-Path $Repo "build\$Preset\mirante.exe"

if ($Build) {
    # Windows locks a running program's file, so the link would fail.
    $running = Get-Process mirante -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe }
    if ($running) {
        throw "Mirante is still running from $Exe (process $($running.Id -join ', ')). Quit it with File > Quit, then run this again."
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -Preset $Preset -NoTest
}

if (-not (Test-Path $Exe)) { throw "$Exe was not found. Build it with tools/build.ps1 -Preset $Preset, or add -Build." }

$QtDir = & (Join-Path $PSScriptRoot 'setup-qt.ps1') | Select-Object -Last 1
$env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"

$AppArgs = @()
# Start-Process joins its arguments with spaces, so the paths are quoted.
if ($Data) { $AppArgs += '--data', "`"$((Resolve-Path $Data).Path)`"" }
# The settings file need not exist yet, so its path is not resolved.
if ($Settings) { $AppArgs += '--settings', "`"$($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Settings))`"" }
if ($Log) { $AppArgs += '--log' }
$Start = @{ FilePath = $Exe }
if ($AppArgs) { $Start.ArgumentList = $AppArgs }
Start-Process @Start
