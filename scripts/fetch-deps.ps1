# Download FFmpeg shared MSVC-compatible binaries into third_party/prebuilt/ffmpeg
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$pre = Join-Path $root "third_party\prebuilt"
New-Item -ItemType Directory -Force -Path $pre | Out-Null

$ffDir = Join-Path $pre "ffmpeg"
if (-not (Test-Path (Join-Path $ffDir "include\libavcodec\avcodec.h"))) {
    $zip = Join-Path $pre "ffmpeg.zip"
    $url = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
    Write-Host "Downloading FFmpeg from $url"
    Invoke-WebRequest -Uri $url -OutFile $zip
    $extract = Join-Path $pre "ffmpeg_extract"
    if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
    Expand-Archive -Path $zip -DestinationPath $extract -Force
    $inner = Get-ChildItem $extract -Directory | Select-Object -First 1
    if (Test-Path $ffDir) { Remove-Item -Recurse -Force $ffDir }
    Move-Item $inner.FullName $ffDir
    Remove-Item $zip -Force
    Remove-Item $extract -Recurse -Force
    Write-Host "FFmpeg installed at $ffDir"
} else {
    Write-Host "FFmpeg already present at $ffDir"
}

Write-Host "OpenSSL: install 'ShiningLight.OpenSSL.Dev' via winget, or set OPENSSL_ROOT_DIR."
Write-Host "Done."
