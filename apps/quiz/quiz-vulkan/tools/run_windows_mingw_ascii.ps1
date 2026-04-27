$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $repoRoot "..\..\..\build\out\quiz\quiz-vulkan\windows-mingw-ascii"
$cmake = "C:\Program Files\CMake\bin\cmake.exe"
$ctest = "C:\Program Files\CMake\bin\ctest.exe"
$commandProcessorKey = "HKCU:\Software\Microsoft\Command Processor"
$mingwBin = "C:\qtmingw1310_ascii\bin"
$hadAutoRun = $false
$oldAutoRun = $null
$exitCode = 0

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Name,
        [Parameter(Mandatory = $true)]
        [string] $FilePath,
        [Parameter(Mandatory = $true)]
        [string[]] $ArgumentList
    )

    Write-Host "==> $Name"
    $process = Start-Process `
        -FilePath $FilePath `
        -ArgumentList $ArgumentList `
        -WorkingDirectory $repoRoot `
        -NoNewWindow `
        -Wait `
        -PassThru
    if ($process.ExitCode -ne 0) {
        throw "$Name failed with exit code $($process.ExitCode)"
    }
}

function Copy-MingwRuntimeDlls {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourceDirectory,
        [Parameter(Mandatory = $true)]
        [string] $DestinationDirectory
    )

    $runtimeDlls = @(
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    )

    foreach ($dll in $runtimeDlls) {
        $source = Join-Path $SourceDirectory $dll
        if (-not (Test-Path -LiteralPath $source)) {
            throw "MinGW runtime DLL missing: $source"
        }

        Copy-Item -LiteralPath $source -Destination $DestinationDirectory -Force
    }
}

try {
    if (Test-Path $commandProcessorKey) {
        $autoRun = Get-ItemProperty -Path $commandProcessorKey -Name AutoRun -ErrorAction SilentlyContinue
        if ($null -ne $autoRun) {
            $hadAutoRun = $true
            $oldAutoRun = $autoRun.AutoRun
            Remove-ItemProperty -Path $commandProcessorKey -Name AutoRun
        }
    }

    $env:PATH = "$mingwBin;C:\qtmingw1310_ascii\x86_64-w64-mingw32\bin;C:\dev\tools\ninja-1.13.2;C:\Program Files\CMake\bin;C:\Windows\System32;C:\Windows"
    Set-Location -LiteralPath $repoRoot

    Invoke-Step "configure" $cmake @("--preset", "windows-mingw-ascii")
    Invoke-Step "build" $cmake @("--build", "--preset", "windows-mingw-ascii-debug")
    Copy-MingwRuntimeDlls -SourceDirectory $mingwBin -DestinationDirectory $buildRoot
    Invoke-Step "test" $ctest @("--test-dir", $buildRoot, "--output-on-failure")
}
catch {
    $exitCode = 1
    Write-Error $_
}
finally {
    if ($hadAutoRun) {
        New-Item -Path $commandProcessorKey -Force | Out-Null
        New-ItemProperty -Path $commandProcessorKey -Name AutoRun -Value $oldAutoRun -PropertyType String -Force | Out-Null
    }
}

exit $exitCode
