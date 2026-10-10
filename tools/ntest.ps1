# Compile + run one host test suite (Unity) with g++. PlatformIO's `pio test -e native` does not
# work for this layout (tests #include modules from src/), so the suites are built directly.
# Works with Windows PowerShell and PowerShell 7 (Windows, Linux, macOS).
#
# Usage (from the repo root): pass the test .cpp first, then the module .cpp files it needs:
#   powershell -ExecutionPolicy Bypass -File tools\ntest.ps1 test\test_theme\test_theme.cpp src\ui\theme.cpp
# tools/run_tests.ps1 runs every suite and works out the module list itself.
#
# Needs g++ on PATH (MinGW-w64 / WinLibs on Windows). If it is installed elsewhere, set
# $env:MINGW_BIN to its bin directory. Unity comes from the native env:
#   pio pkg install -e native -l throwtheswitch/Unity@2.6.0
# Exit code 0 = all tests passed.
$ErrorActionPreference = "Stop"
if ($env:MINGW_BIN) { $env:Path = $env:MINGW_BIN + [IO.Path]::PathSeparator + $env:Path }
$root  = Split-Path $PSScriptRoot -Parent
$unity = [IO.Path]::Combine($root, ".pio", "libdeps", "native", "Unity", "src")
if (-not (Test-Path (Join-Path $unity "unity.c"))) {
    Write-Host "Unity source not found at $unity"
    Write-Host "Fetch it once:  pio pkg install -e native -l throwtheswitch/Unity@2.6.0"
    exit 2
}
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Host "g++ not found: install MinGW-w64 / GCC or set `$env:MINGW_BIN to its bin directory."
    exit 2
}
if ($args.Count -lt 1) { Write-Host "Usage: ntest.ps1 <test.cpp> [module.cpp ...]"; exit 2 }
$out = Join-Path ([IO.Path]::GetTempPath()) ("ntest_" + [IO.Path]::GetFileNameWithoutExtension($args[0]) + ".exe")
$ccargs = @("-std=gnu++17", "-Wall", "-Wextra", "-Wshadow", "-Werror", "-I", $unity) + $args + @((Join-Path $unity "unity.c"), "-o", $out)
# On Linux (CI) the sanitizers come free: undefined behaviour and memory errors fail the suite.
# NTEST_SAN=0 turns them off; NTEST_FLAGS adds flags (CI's 32-bit run uses -m32: the ESP32's
# size_t is 32-bit, which is where offset + size wraps).
if ($IsLinux -and $env:NTEST_SAN -ne "0") {
    $ccargs += @("-g", "-fsanitize=address,undefined,float-cast-overflow", "-fno-sanitize-recover=all")
}
if ($env:NTEST_FLAGS) { $ccargs += ($env:NTEST_FLAGS -split '\s+' | Where-Object { $_ }) }
& g++ @ccargs
if ($LASTEXITCODE -ne 0) { Write-Host "COMPILE FAILED"; exit 1 }
& $out
exit $LASTEXITCODE
