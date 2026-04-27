[CmdletBinding()]
param(
    [string]$Compiler = $env:CXX,
    [string]$BuildRoot = "tests\out",
    [switch]$KeepBinaries
)

$ErrorActionPreference = "Stop"

$repo_root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repo_root

if (-not $Compiler) {
    foreach ($candidate in @("C:\MinGW\bin\g++.exe", "g++.exe", "g++")) {
        if (Test-Path $candidate) {
            $Compiler = (Resolve-Path $candidate).Path
            break
        }

        $command = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($command) {
            $Compiler = $command.Source
            break
        }
    }
}

if (-not $Compiler) {
    throw "MinGW g++ was not found. Set CXX to C:\MinGW\bin\g++.exe or pass -Compiler."
}

$build_root_path = Join-Path $repo_root $BuildRoot
New-Item -ItemType Directory -Force -Path $build_root_path | Out-Null
$build_dir = Join-Path $build_root_path ("mingw-scene-layout-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $build_dir | Out-Null

function Invoke-CheckedProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$FailureMessage
    )

    if ($Arguments.Count -gt 0) {
        $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments -NoNewWindow -Wait -PassThru
    } else {
        $process = Start-Process -FilePath $FilePath -NoNewWindow -Wait -PassThru
    }
    if ($process.ExitCode -ne 0) {
        throw "$FailureMessage (exit $($process.ExitCode))"
    }
}

function Invoke-SmokeTest {
    param(
        [string]$Source,
        [string]$Name
    )

    $exe = Join-Path $build_dir ($Name + ".exe")
    Invoke-CheckedProcess `
        -FilePath $Compiler `
        -Arguments @("-std=c++1z", "-Wall", "-Wextra", "-pedantic", "-Isrc", $Source, "-o", $exe) `
        -FailureMessage "compile failed: $Name"

    if (-not (Test-Path $exe)) {
        throw "compile failed: $Name"
    }

    $resolved_exe = (Resolve-Path $exe).Path
    Invoke-CheckedProcess -FilePath $resolved_exe -Arguments @() -FailureMessage "test failed: $Name"

    Write-Host "passed $Name"
}

try {
    Write-Host "compiler $Compiler"
    Invoke-SmokeTest -Source "tests\scene\scene_layout_data_tests.cpp" -Name "scene_layout_data_tests"
    Invoke-SmokeTest -Source "tests\layout\layout_placer_tests.cpp" -Name "layout_placer_tests"
}
finally {
    if (-not $KeepBinaries) {
        Remove-Item -Recurse -Force $build_dir -ErrorAction SilentlyContinue
    }
}
