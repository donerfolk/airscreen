$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

& "$PSScriptRoot\fetch-deps.ps1"

$cmake = @(
    "$env:ProgramFiles\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cmake) {
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
}
if (-not $cmake) {
    throw "CMake not found. Install Kitware.CMake via winget."
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$generator = "Visual Studio 17 2022"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) {
        Write-Warning "Visual Studio C++ tools not found. Install VS 2022 Desktop C++ workload."
    }
}

$openssl = @(
    "$env:ProgramFiles\OpenSSL-Win64",
    "$env:ProgramFiles\OpenSSL",
    "C:\OpenSSL-Win64"
) | Where-Object { Test-Path "$_\include\openssl\ssl.h" } | Select-Object -First 1

$args = @("-S", $root, "-B", "$root\build", "-G", $generator, "-A", "x64")
if ($openssl) {
    $args += "-DOPENSSL_ROOT_DIR=$openssl"
}

Write-Host "Configuring: $cmake $($args -join ' ')"
& $cmake @args
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& $cmake --build "$root\build" --config Release
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Write-Host "Built: $root\build\Release\AirScreen.exe"
