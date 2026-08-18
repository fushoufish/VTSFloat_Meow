param(
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$NativeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $VsWhere)) {
    throw "Visual Studio Build Tools not found."
}

$InstallDir = & $VsWhere `
    -latest `
    -products Microsoft.VisualStudio.Product.BuildTools `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $InstallDir) {
    throw "The MSVC x64 toolchain is not installed."
}

$CMake = Join-Path $InstallDir "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$BuildDir = Join-Path $NativeDir "build"

# A project folder can be renamed after a previous build.  CMake stores the
# absolute source path in its cache, so discard only this generated build
# directory when it points at the old location.
$CacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path -LiteralPath $CacheFile) {
    $expectedSource = ($NativeDir -replace '\\', '/')
    $cachedHome = Select-String -LiteralPath $CacheFile `
        -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$' |
        Select-Object -First 1
    if ($cachedHome -and $cachedHome.Matches[0].Groups[1].Value -ne $expectedSource) {
        Remove-Item -LiteralPath $BuildDir -Recurse -Force
    }
}

& $CMake -S $NativeDir -B $BuildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $CMake --build $BuildDir --config $Configuration --parallel
exit $LASTEXITCODE
