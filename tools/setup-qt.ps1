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
# A path such as F:Qt is relative to the current folder on drive F, so Qt
# would land in whatever folder the shell is in.
if ($env:MIRANTE_QT_ROOT -and $env:MIRANTE_QT_ROOT -notmatch '^([A-Za-z]:[\\/]|[\\/]{2})') {
    throw "MIRANTE_QT_ROOT must be a full path such as F:\Qt, but it is '$env:MIRANTE_QT_ROOT'."
}
$QtDir = Join-Path $QtRoot "$QtVersion\$QtArchDir"

# qtsvg provides the plugin that loads the SVG icons of item types.
# Qt WebEngine shows web items; it needs qtdeclarative, qtwebchannel and
# qtpositioning. qttools has lupdate and lrelease for the translations, and
# qttranslations has Qt's own texts in other languages. qtimageformats has the
# WebP plugin for dropped images. qtpdf shows the PDF files of document items.
# Each part has a file that shows it is there, so an older Qt without one of
# them is completed.
$Markers = @(
    'lib\cmake\Qt6WebEngineWidgets',
    'bin\lupdate.exe',
    'translations\qtbase_pt_BR.qm',
    'plugins\imageformats\qwebp.dll',
    'lib\cmake\Qt6PdfWidgets'
)
$Missing = @($Markers | Where-Object { -not (Test-Path (Join-Path $QtDir $_)) })
if ($Missing) {
    if (-not (Get-Command uvx -ErrorAction SilentlyContinue)) {
        throw 'uv is required to download Qt. Install it from https://docs.astral.sh/uv/ and run this script again.'
    }
    Write-Host "Installing Qt $QtVersion into $QtRoot"
    # uvx and aqt print progress on stderr, which Windows PowerShell turns into
    # errors. Pass it on as text and check the exit code instead.
    $ErrorActionPreference = 'Continue'
    & uvx --from aqtinstall aqt install-qt windows desktop $QtVersion $QtArch -O $QtRoot --archives qtbase qtsvg qtdeclarative qttools qttranslations -m qtwebengine qtwebchannel qtpositioning qtimageformats qtpdf 2>&1 |
        ForEach-Object { "$_" } | Out-Host
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { throw "aqtinstall failed with exit code $LASTEXITCODE" }
}

$QtDir
