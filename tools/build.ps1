# Configures, builds and tests one CMake preset on Windows, from any shell.
#
#   tools/build.ps1                  the dev preset
#   tools/build.ps1 -Preset release  another preset
#   tools/build.ps1 -NoTest          build only
#   tools/build.ps1 -Target <name>   build one target only, without tests,
#                                    such as update_translations
#   tools/build.ps1 -Version 1.2.0   build that version, as the release does
#   tools/build.ps1 -NoPch           compile without precompiled headers, as CI
#                                    does, so a missing include fails
#   tools/build.ps1 -Package         after the tests, install into
#                                    build/<preset>/install, check the install
#                                    and make the setup with CPack
#
# Downloads Qt with tools/setup-qt.ps1 if it is missing, and enters the
# Visual Studio 2022 developer environment if the shell is not in one.

param(
    [string]$Preset = 'dev',
    [switch]$NoTest,
    [string]$Target,
    [string]$Version,
    [switch]$NoPch,
    [switch]$Package
)

$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot

# Windows locks a running program's file, so the build would fail near its end.
# Closing the window can leave Mirante running, hidden in the tray.
$BuildDir = Join-Path $Repo "build\$Preset\"
$running = Get-Process mirante -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($BuildDir, [StringComparison]::OrdinalIgnoreCase) }
if ($running) {
    throw "Mirante is still running from $($running[0].Path) (process $($running.Id -join ', ')), so the build cannot replace it. Quit it with File > Quit, or from its tray icon, then build again."
}

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
    $configure = @('--preset', $Preset, "-DCMAKE_PREFIX_PATH=$QtDir")
    if ($Version) { $configure += "-DMIRANTE_VERSION=$Version" }
    # Set each time, because the build folder keeps the value of its last run.
    $configure += "-DMIRANTE_PCH=$(if ($NoPch) { 'OFF' } else { 'ON' })"
    Invoke-Tool cmake @configure
    if ($Target) {
        Invoke-Tool cmake --build --preset $Preset --target $Target
        return
    }
    Invoke-Tool cmake --build --preset $Preset
    if (-not $NoTest) {
        # The tests load the Qt DLLs from Qt's bin folder.
        $env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
        # The tests use temporary folders of their own, so they run in
        # parallel, one per core unless CTEST_PARALLEL_LEVEL says otherwise.
        if (-not $env:CTEST_PARALLEL_LEVEL) { $env:CTEST_PARALLEL_LEVEL = [Environment]::ProcessorCount }
        Invoke-Tool ctest --preset $Preset
    }
    if ($Package) {
        $installDir = Join-Path $BuildDir 'install'
        if (Test-Path $installDir) { Remove-Item -Recurse -Force $installDir }
        Invoke-Tool cmake --install $BuildDir --component Mirante --prefix $installDir
        Invoke-Tool python (Join-Path $PSScriptRoot 'check-package.py') $installDir
        Push-Location $BuildDir
        try { Invoke-Tool cpack } finally { Pop-Location }
    }
}
finally {
    Pop-Location
}
