param(
    [string]$CppCommand,
    [string]$PlanCommand,
    [ValidateSet("ask-user", "prefer-local-readonly", "direct-only")]
    [string]$DelegationMode,
    [string]$RuntimeSource
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-DefaultValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Prompt,
        [Parameter(Mandatory = $true)]
        [string]$DefaultValue
    )

    $answer = Read-Host "$Prompt [$DefaultValue]"
    if ([string]::IsNullOrWhiteSpace($answer)) {
        return $DefaultValue
    }
    return $answer.Trim()
}

function Test-AsciiPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    foreach ($char in $PathValue.ToCharArray()) {
        if ([int][char]$char -gt 127) {
            return $false
        }
    }
    return $true
}

function Resolve-PythonLauncher {
    $durablePython = "C:\codex-local\runtime\python\python.exe"
    if (Test-Path $durablePython) {
        return @{
            Command = $durablePython
            PrefixArgs = @()
        }
    }

    $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
    if ($pyLauncher) {
        return @{
            Command = $pyLauncher.Source
            PrefixArgs = @("-3")
        }
    }

    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return @{
            Command = $python.Source
            PrefixArgs = @()
        }
    }

    throw "Could not find a Python launcher. Install Python or provide C:\codex-local\runtime\python first."
}

function Detect-RuntimeRoot {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Launcher
    )

    $queryArgs = @($Launcher.PrefixArgs + @("-c", "import sys; from pathlib import Path; print(Path(sys.executable).resolve().parent.parent)"))
    $detectedRoot = & $Launcher.Command @queryArgs
    if ($LASTEXITCODE -ne 0) {
        return $null
    }

    $resolved = ($detectedRoot | Select-Object -First 1).Trim()
    if (-not $resolved) {
        return $null
    }

    if (-not (Test-Path (Join-Path $resolved "python.exe"))) {
        return $null
    }

    if (-not (Test-AsciiPath $resolved)) {
        return $null
    }

    return $resolved
}

if (-not $CppCommand) {
    $CppCommand = Read-DefaultValue -Prompt "Scaffold command name" -DefaultValue "cpp-new"
}
if (-not $PlanCommand) {
    $PlanCommand = Read-DefaultValue -Prompt "Plan document command name" -DefaultValue "plan-doc"
}
if (-not $DelegationMode) {
    $DelegationMode = Read-DefaultValue -Prompt "Low-risk local delegation mode (ask-user|prefer-local-readonly|direct-only)" -DefaultValue "ask-user"
}

$launcher = Resolve-PythonLauncher
$durableRuntime = "C:\codex-local\runtime\python"

if (-not $RuntimeSource -and -not (Test-Path (Join-Path $durableRuntime "python.exe"))) {
    $detectedRuntime = Detect-RuntimeRoot -Launcher $launcher
    if ($detectedRuntime) {
        $RuntimeSource = $detectedRuntime
    } else {
        $RuntimeSource = Read-Host "ASCII Python runtime root to copy into C:\codex-local\runtime\python"
    }
}

if ($RuntimeSource) {
    if (-not (Test-Path $RuntimeSource)) {
        throw "Runtime source does not exist: $RuntimeSource"
    }
    if (-not (Test-AsciiPath $RuntimeSource)) {
        throw "Runtime source must use an ASCII-only path: $RuntimeSource"
    }
}

$installer = Join-Path $PSScriptRoot "install_codex_env.py"
$args = @()
$args += $launcher.PrefixArgs
$args += @(
    $installer,
    "--cpp-command", $CppCommand,
    "--plan-command", $PlanCommand,
    "--delegation-mode", $DelegationMode
)

if ($RuntimeSource) {
    $args += @("--runtime-source", $RuntimeSource)
}

& $launcher.Command @args
exit $LASTEXITCODE
