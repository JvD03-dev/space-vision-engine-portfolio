param(
    [string]$Config = "configs/scenarios/rendezvous_glint_fast_iter.json",
    [double]$Speed = 80.0,
    [double]$Fps = 60.0,
    [string]$BuildDir = "build-cpp-min",
    [string]$BuildType = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Set-Location -LiteralPath $PSScriptRoot

Write-Host "[cpp-engine] configuring CMake..."
cmake -S . -B $BuildDir

Write-Host "[cpp-engine] building..."
cmake --build $BuildDir --config $BuildType --target space_vision_engine

$exe = Get-ChildItem -Path $BuildDir -Recurse -File `
    | Where-Object { $_.Name -eq "space_vision_engine.exe" -or $_.Name -eq "space_vision_engine" } `
    | Sort-Object LastWriteTime -Descending `
    | Select-Object -First 1

if (-not $exe) {
    throw "Could not find space_vision_engine binary in $BuildDir."
}

Write-Host "[cpp-engine] starting $($exe.FullName)"
& $exe.FullName --config $Config --speed $Speed --fps $Fps
