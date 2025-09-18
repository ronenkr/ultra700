<#!
.SYNOPSIS
  Convenience build script for ultra700 (bootloader + payload) using CMake presets or manual fallback.
.DESCRIPTION
  Ensures Ninja + toolchain configuration with relative paths only.
.PARAMETER Config
  Build configuration: Release (default) or Debug.
.PARAMETER Clean
  If specified, removes the build directory (matching config) before configuring.
.PARAMETER Reconfigure
  Force reconfigure (delete CMakeCache.txt) without wiping entire directory.
.PARAMETER Target
  Specific target to build (default builds all executables). Examples: payload.elf, bootloader.elf.
.PARAMETER Preset
  Explicit preset name (overrides Config logic). e.g. arm-release, arm-debug.
.PARAMETER Monitor
  If specified, runs monitor.ps1 after a successful build.
.EXAMPLE
  ./build.ps1
.EXAMPLE
  ./build.ps1 -Config Debug -Target payload.elf
.EXAMPLE
  ./build.ps1 -Clean -Config Release
#>
[CmdletBinding()] param(
  [ValidateSet('Release','Debug')][string]$Config = 'Release',
  [switch]$Clean,
  [switch]$Reconfigure,
  [string]$Target,
  [string]$Preset,
  [switch]$Monitor
)

$ErrorActionPreference = 'Stop'
$Script:Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Script:Root

$usePreset = $false
if ($Preset) { $usePreset = $true } else {
  if ($Config -eq 'Release') { $Preset = 'arm-release' } else { $Preset = 'arm-debug' }
}

$buildDir = if ($Preset -eq 'arm-debug') { 'build-debug' } else { 'build' }

if ($Clean) {
  if (Test-Path $buildDir) { Write-Host "[clean] Removing $buildDir"; Remove-Item -Recurse -Force $buildDir }
}

if (!(Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

# Decide if configure needed
$needConfigure = $false
if (!(Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) { $needConfigure = $true }
if ($Reconfigure -and (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) { Remove-Item (Join-Path $buildDir 'CMakeCache.txt'); $needConfigure = $true }

# Prefer presets if available
$cmakeExe = 'cmake'

if ($usePreset -or (Test-Path (Join-Path $Root 'CMakePresets.json'))) {
  if ($needConfigure) {
    Write-Host "[configure] cmake --preset $Preset"
    & $cmakeExe --preset $Preset
  } else {
    Write-Host "[configure] (cached) $Preset"
  }
} else {
  # Manual fallback (relative paths only)
  if ($needConfigure) {
    $toolchainRel = 'cmake/toolchain-arm-none-eabi.cmake'
    $ninjaRel = 'tools/ninja/ninja.exe'
    Write-Host "[configure] (manual) $Config"
    & $cmakeExe -S . -B $buildDir -G Ninja -DCMAKE_MAKE_PROGRAM=$ninjaRel -DCMAKE_TOOLCHAIN_FILE=$toolchainRel -DCMAKE_BUILD_TYPE=$Config
  } else {
    Write-Host "[configure] (manual cached) $Config"
  }
}

# Build
if ($Target) {
  if ($usePreset -or (Test-Path (Join-Path $Root 'CMakePresets.json'))) {
    Write-Host "[build] cmake --build --preset $Preset --target $Target"
    & $cmakeExe --build --preset $Preset --target $Target
  } else {
    Write-Host "[build] cmake --build $buildDir --target $Target"
    & $cmakeExe --build $buildDir --target $Target
  }
} else {
  if ($usePreset -or (Test-Path (Join-Path $Root 'CMakePresets.json'))) {
    Write-Host "[build] cmake --build --preset $Preset"
    & $cmakeExe --build --preset $Preset
  } else {
    Write-Host "[build] cmake --build $buildDir"
    & $cmakeExe --build $buildDir
  }
}

if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }

Write-Host "[done] Outputs in bin/ (payload.elf, bootloader.elf, *.bin, *.hex)"

if ($Monitor) {
  $monitorScript = Join-Path $Root 'monitor.ps1'
  if (Test-Path $monitorScript) {
    Write-Host "[monitor] launching monitor.ps1"
    try {
      powershell -ExecutionPolicy Bypass -File $monitorScript
    } catch {
      Write-Warning "[monitor] monitor.ps1 failed: $_"
    }
  } else {
    Write-Warning "[monitor] monitor.ps1 not found"
  }
}
