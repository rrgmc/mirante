# Downloads the prebuilt Qt that Mirante builds against, with aqtinstall.
# Requires uv (https://docs.astral.sh/uv/).
#
# Qt goes into the folder named by the environment variable MIRANTE_QT_ROOT,
# or into .qt/ in the repository if it is not set. Several projects can share
# one Qt by setting MIRANTE_QT_ROOT to the same folder.
#
# Prints the Qt folder to pass to CMake as CMAKE_PREFIX_PATH.

$ErrorActionPreference = 'Stop'

$QtVersion = '6.8.3'
$QtArch = 'win64_msvc2022_64'
$QtArchDir = 'msvc2022_64'

$Repo = Split-Path -Parent $PSScriptRoot
$QtRoot = if ($env:MIRANTE_QT_ROOT) { $env:MIRANTE_QT_ROOT } else { Join-Path $Repo '.qt' }
$QtDir = Join-Path $QtRoot "$QtVersion\$QtArchDir"

# qtsvg provides the plugin that loads the SVG icons of item types.
if (-not (Test-Path (Join-Path $QtDir 'lib\cmake\Qt6Svg'))) {
    if (-not (Get-Command uvx -ErrorAction SilentlyContinue)) {
        throw 'uv is required to download Qt. Install it from https://docs.astral.sh/uv/ and run this script again.'
    }
    Write-Host "Installing Qt $QtVersion into $QtRoot"
    # uvx and aqt print progress on stderr, which Windows PowerShell turns into
    # errors. Pass it on as text and check the exit code instead.
    $ErrorActionPreference = 'Continue'
    & uvx --from aqtinstall aqt install-qt windows desktop $QtVersion $QtArch -O $QtRoot --archives qtbase qtsvg 2>&1 |
        ForEach-Object { "$_" } | Out-Host
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { throw "aqtinstall failed with exit code $LASTEXITCODE" }
}

$QtDir
