# Configures, builds and tests one CMake preset on Windows, from any shell.
#
#   tools/build.ps1                  the dev preset
#   tools/build.ps1 -Preset release  another preset
#   tools/build.ps1 -NoTest          build only
#
# Downloads Qt with tools/setup-qt.ps1 if it is missing, and enters the
# Visual Studio 2022 developer environment if the shell is not in one.

param(
    [string]$Preset = 'dev',
    [switch]$NoTest
)

$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot

$QtDir = & (Join-Path $PSScriptRoot 'setup-qt.ps1') | Select-Object -Last 1

if (-not $env:VSCMD_VER) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'Visual Studio 2022 is required, and vswhere.exe was not found.' }
    $vs = & $vswhere -latest -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio 2022 with the C++ tools is required.' }
    # The developer shell looks for vswhere.exe on PATH.
    $env:PATH = "$(Split-Path -Parent $vswhere);$env:PATH"
    Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -Arch amd64 -HostArch amd64 | Out-Null
}

# Runs a tool and fails on a non-zero exit code. Windows PowerShell turns a
# native program's stderr output into error records, and CMake prints warnings
# there, so the output is passed on as plain text instead.
function Invoke-Tool {
    $ErrorActionPreference = 'Continue'
    & $args[0] $args[1..($args.Count - 1)] 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "$($args[0]) failed with exit code $LASTEXITCODE" }
}

Push-Location $Repo
try {
    Invoke-Tool cmake --preset $Preset "-DCMAKE_PREFIX_PATH=$QtDir"
    Invoke-Tool cmake --build --preset $Preset
    if (-not $NoTest) {
        # The tests load the Qt DLLs from Qt's bin folder.
        $env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
        Invoke-Tool ctest --preset $Preset
    }
}
finally {
    Pop-Location
}
