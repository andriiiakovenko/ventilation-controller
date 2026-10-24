<#
.SYNOPSIS
    Builds the ESP32 Ventilation Controller Master and Slave firmware images with versioning and timestamping.
.DESCRIPTION
    Compiles PlatformIO targets and exports binaries into the builds/ directory:
      - Master: ventmaster_<version>_<yyyyMMdd_HHmmss>.bin
      - Slaves: ventslave_<slave name>_<version>_<yyyyMMdd_HHmmss>.bin
.PARAMETER Target
    Build target: "all" (default), "master", or "slaves".
.PARAMETER Slaves
    Array of slave room names to build (default: "danylo", "anastasiia", "parents", "gim").
.PARAMETER Slave
    Optional single slave room name to build (e.g. "parents" or "gim").
.PARAMETER Version
    Optional firmware version string (e.g. "0.0.1"). Defaults to version found in include/config.h.
#>

param(
    [ValidateSet("all", "master", "slaves")]
    [string]$Target = "all",

    [string[]]$Slaves = @("danylo", "anastasiia", "parents", "gim"),

    [string]$Slave = "",

    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

# Navigate to script root
$rootDir = $PSScriptRoot
Set-Location $rootDir

# Extract version from include/config.h if not specified
$configHeader = Join-Path $rootDir "include\config.h"
if ([string]::IsNullOrWhiteSpace($Version) -and (Test-Path $configHeader)) {
    $match = Select-String -Path $configHeader -Pattern '#define\s+FIRMWARE_VERSION\s+"([^"]+)"'
    if ($match -and $match.Matches.Groups.Count -gt 1) {
        $Version = $match.Matches.Groups[1].Value
    }
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = "0.0.1"
}

# If a single slave was requested, override the array
if (![string]::IsNullOrWhiteSpace($Slave)) {
    $Slaves = @($Slave)
    if ($Target -eq "master") {
        Write-Warning "Specified both -Target master and -Slave '$Slave'. Will build slave '$Slave'."
        $Target = "slaves"
    } elseif ($Target -eq "all") {
        $Target = "slaves"
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$buildsDir = Join-Path $rootDir "builds"
if (!(Test-Path $buildsDir)) {
    New-Item -ItemType Directory -Path $buildsDir | Out-Null
}

# Locate PlatformIO executable
$pioExe = "pio"
if (!(Get-Command "pio" -ErrorAction SilentlyContinue)) {
    $fallbackPio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
    if (Test-Path $fallbackPio) {
        $pioExe = $fallbackPio
    } else {
        Write-Error "PlatformIO executable not found in PATH or at $fallbackPio"
        exit 1
    }
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " ESP32 Ventilation Controller - Firmware Builder" -ForegroundColor Cyan
Write-Host " Target:    $Target" -ForegroundColor Cyan
Write-Host " Version:   v$Version" -ForegroundColor Cyan
Write-Host " Timestamp: $timestamp" -ForegroundColor Cyan
Write-Host " Output:    $buildsDir" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$exportedFiles = @()

function Test-FirmwareTargetDescriptor {
    param(
        [string]$BinPath,
        [string]$ExpectedRole,
        [string]$ExpectedTargetId,
        [uint16]$ExpectedChipId
    )
    $bytes = [System.IO.File]::ReadAllBytes($BinPath)
    if ($bytes.Length -lt 352) {
        Write-Error "Binary $BinPath is too small ($($bytes.Length) bytes)"
        exit 1
    }
    if ($bytes[0] -ne 0xE9) {
        Write-Error "Binary $BinPath missing ESP32 0xE9 magic byte"
        exit 1
    }
    $chipId = [uint16]$bytes[12] -bor ([uint16]$bytes[13] -shl 8)
    if ($chipId -ne $ExpectedChipId) {
        Write-Error "Binary $BinPath chip_id mismatch: got 0x$($chipId.ToString('X4')), expected 0x$($ExpectedChipId.ToString('X4'))"
        exit 1
    }
    $magic = ([System.Text.Encoding]::ASCII.GetString($bytes[288..303]) -split "`0")[0]
    $role  = ([System.Text.Encoding]::ASCII.GetString($bytes[304..319]) -split "`0")[0]
    $tid   = ([System.Text.Encoding]::ASCII.GetString($bytes[320..335]) -split "`0")[0]
    $ver   = ([System.Text.Encoding]::ASCII.GetString($bytes[336..351]) -split "`0")[0]

    if ($magic -ne "VENT_FW_DESC_V1") {
        Write-Error "Binary $BinPath missing VENT_FW_DESC_V1 signature at offset 288 (got '$magic')"
        exit 1
    }
    if ($role -ne $ExpectedRole -or $tid -ne $ExpectedTargetId) {
        Write-Error "Binary $BinPath target descriptor mismatch: got '${role}:${tid}', expected '${ExpectedRole}:${ExpectedTargetId}'"
        exit 1
    }
    Write-Host "  -> Verified Embedded Descriptor: magic=$magic, target=${role}:${tid}, version=$ver" -ForegroundColor DarkCyan
    return "${role}:${tid}"
}

# ==========================================
# 1. BUILD MASTER (Main Controller)
# ==========================================
if ($Target -eq "all" -or $Target -eq "master") {
    Write-Host "`n>>> [Building Master Controller] (Ventilation Master - ESP32)..." -ForegroundColor Yellow
    
    # Clear any slave build flags
    Remove-Item Env:\PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue

    & $pioExe run
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Master firmware compilation failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    $masterSrc = Join-Path $rootDir ".pio\build\esp32dev\firmware.bin"
    if (!(Test-Path $masterSrc)) {
        Write-Error "Master binary not found at $masterSrc"
        exit 1
    }

    $verifiedTarget = Test-FirmwareTargetDescriptor -BinPath $masterSrc -ExpectedRole "master" -ExpectedTargetId "master" -ExpectedChipId 0x0000

    $masterName = "ventmaster_${Version}_${timestamp}.bin"
    $masterDest = Join-Path $buildsDir $masterName
    Copy-Item -Path $masterSrc -Destination $masterDest -Force

    $masterLatest = Join-Path $buildsDir "ventmaster_latest.bin"
    Copy-Item -Path $masterSrc -Destination $masterLatest -Force

    $masterSizeKB = [math]::Round(((Get-Item $masterDest).Length / 1KB), 1)
    $exportedFiles += [PSCustomObject]@{
        Module     = "Master"
        Target     = "ESP32 Dev"
        FwIdentity = $verifiedTarget
        File       = $masterName
        SizeKB     = $masterSizeKB
    }
    Write-Host "  -> Master build OK: $masterName ($masterSizeKB KB)" -ForegroundColor Green
}

# ==========================================
# 2. BUILD SLAVES (Sensor Nodes)
# ==========================================
if ($Target -eq "all" -or $Target -eq "slaves") {
    $sensorNodeDir = Join-Path $rootDir "sensor_node_example"

    if (!(Test-Path $sensorNodeDir)) {
        Write-Error "Sensor node project directory not found at $sensorNodeDir"
        exit 1
    }

    foreach ($slaveName in $Slaves) {
        $sNameClean = $slaveName.Trim().ToLower()
        Write-Host "`n>>> [Building Slave Node: $sNameClean] (ESP32-C3 Super Mini)..." -ForegroundColor Yellow

        # Inject ROOM_ID and FIRMWARE_VERSION via PlatformIO build flags
        $env:PLATFORMIO_BUILD_FLAGS = "-D ROOM_ID=\`"$sNameClean\`" -D FIRMWARE_VERSION=\`"$Version\`""

        & $pioExe run -d $sensorNodeDir
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Slave '$sNameClean' compilation failed with exit code $LASTEXITCODE"
            exit $LASTEXITCODE
        }

        $slaveSrc = Join-Path $sensorNodeDir ".pio\build\esp32-c3\firmware.bin"
        if (!(Test-Path $slaveSrc)) {
            Write-Error "Slave binary not found at $slaveSrc"
            exit 1
        }

        $verifiedSlaveTarget = Test-FirmwareTargetDescriptor -BinPath $slaveSrc -ExpectedRole "slave" -ExpectedTargetId $sNameClean -ExpectedChipId 0x0005

        # Naming format: ventslave_[slave name]_version_date_time
        $slaveFileName = "ventslave_${sNameClean}_${Version}_${timestamp}.bin"
        $slaveDest = Join-Path $buildsDir $slaveFileName
        Copy-Item -Path $slaveSrc -Destination $slaveDest -Force

        # Also copy latest version for easy testing
        $slaveLatest = Join-Path $buildsDir "ventslave_${sNameClean}_latest.bin"
        Copy-Item -Path $slaveSrc -Destination $slaveLatest -Force

        $slaveSizeKB = [math]::Round(((Get-Item $slaveDest).Length / 1KB), 1)
        $exportedFiles += [PSCustomObject]@{
            Module     = "Slave ($sNameClean)"
            Target     = "ESP32-C3"
            FwIdentity = $verifiedSlaveTarget
            File       = $slaveFileName
            SizeKB     = $slaveSizeKB
        }
        Write-Host "  -> Slave [$sNameClean] build OK: $slaveFileName ($slaveSizeKB KB)" -ForegroundColor Green
    }

    # Clean up environment variable
    Remove-Item Env:\PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue
}

# ==========================================
# SUMMARY OF GENERATED BINARIES
# ==========================================
Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host " BUILD SUMMARY - All Binaries Exported Successfully" -ForegroundColor Green
Write-Host " Output Directory: $buildsDir" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green

$exportedFiles | Format-Table -AutoSize

Write-Host "Ready to flash via Web OTA Administration Dashboard or USB!`n" -ForegroundColor Cyan
