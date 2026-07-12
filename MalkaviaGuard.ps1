# MalkaviaGuard - Advanced Anti-Cheat Monitor for Roblox
# Detects: DLL Injection, Memory Anomalies, Suspicious Threads, Process Access Patterns

param(
    [string]$WebhookUrl = "https://canary.discord.com/api/webhooks/1525718562411516016/GvGdbw1xrdL8X9zL9j4UGgqF13lxmuYqd0VkKcPw2VenOzF4f4Lmkp4G6QU9gOciIYuS",
    [int]$CheckInterval = 3000
)

$ErrorActionPreference = 'SilentlyContinue'
$global:RobloxPID = $null
$global:BaselineModules = @{}
$global:BaselineThreads = @{}
$global:SuspiciousModules = @()
$global:DetectionLog = @()

# ============= DISCORD INTEGRATION =============
function Send-DiscordAlert {
    param(
        [string]$Title,
        [string]$Description,
        [string]$Severity = "LOW",
        [hashtable]$Details = @{}
    )
    
    $severityColors = @{
        "CRITICAL" = 16711680;  # Red
        "HIGH"     = 16750592;  # Orange
        "MEDIUM"   = 16776960;  # Yellow
        "LOW"      = 3329330    # Green
    }
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $fields = @()
    
    $fields += @{
        name  = "Timestamp"
        value = $timestamp
        inline = $true
    }
    
    $fields += @{
        name  = "Process ID"
        value = $global:RobloxPID
        inline = $true
    }
    
    $fields += @{
        name  = "Severity"
        value = $Severity
        inline = $true
    }
    
    foreach ($key in $Details.Keys) {
        $fields += @{
            name  = $key
            value = $Details[$key]
            inline = $false
        }
    }
    
    $embed = @{
        title       = "🚨 $Title"
        description = $Description
        color       = $severityColors[$Severity]
        fields      = $fields
        footer      = @{
            text = "MalkaviaGuard v1.0"
        }
    }
    
    $payload = @{
        embeds = @($embed)
    } | ConvertTo-Json -Depth 10
    
    try {
        Invoke-WebRequest -Uri $WebhookUrl -Method POST -Body $payload -ContentType "application/json" -UseBasicParsing -TimeoutSec 5 | Out-Null
        Write-Host "[DISCORD] Alert sent: $Title" -ForegroundColor Cyan
    } catch {
        Write-Host "[ERROR] Failed to send Discord alert: $_" -ForegroundColor Red
    }
}

# ============= PROCESS DETECTION =============
function Find-RobloxProcess {
    $process = Get-Process -Name "RobloxPlayerBeta" -ErrorAction SilentlyContinue
    
    if ($process) {
        if ($global:RobloxPID -ne $process.Id) {
            $global:RobloxPID = $process.Id
            Write-Host "[DETECTION] Roblox process started: PID $($process.Id)" -ForegroundColor Green
            Send-DiscordAlert -Title "Roblox Process Started" -Description "RobloxPlayerBeta.exe detected" -Severity "LOW" -Details @{
                "Process ID" = $process.Id
                "Process Name" = $process.ProcessName
            }
            return $true
        }
        return $true
    } else {
        if ($global:RobloxPID) {
            Write-Host "[INFO] Roblox process terminated" -ForegroundColor Yellow
            $global:RobloxPID = $null
            $global:BaselineModules = @{}
            $global:BaselineThreads = @{}
        }
        return $false
    }
}

# ============= DLL INJECTION DETECTION =============
function Detect-DLLInjection {
    if (-not $global:RobloxPID) { return }
    
    try {
        $process = Get-Process -Id $global:RobloxPID -ErrorAction SilentlyContinue
        if (-not $process) { return }
        
        # Get all loaded modules
        $modules = $process.Modules | Select-Object -ExpandProperty ModuleName | Sort-Object
        
        # Baseline initialization
        if ($global:BaselineModules.Count -eq 0) {
            foreach ($module in $modules) {
                $global:BaselineModules[$module] = $true
            }
            Write-Host "[DLL_MONITOR] Baseline established: $($modules.Count) modules" -ForegroundColor Green
            return
        }
        
        # Detect new modules (potential injections)
        $newModules = @()
        foreach ($module in $modules) {
            if (-not $global:BaselineModules.ContainsKey($module)) {
                $newModules += $module
                
                # Check if DLL is signed and in standard paths
                $isStandard = Test-StandardDLL -DLLPath $module
                $severity = if ($isStandard) { "LOW" } else { "HIGH" }
                
                $details = @{
                    "DLL Name" = Split-Path -Leaf $module
                    "Full Path" = $module
                    "Signed" = (if ($isStandard) { "Yes" } else { "No" })
                }
                
                Write-Host "[INJECTION] New DLL detected: $module (Severity: $severity)" -ForegroundColor Yellow
                Send-DiscordAlert -Title "DLL Injection Detected" -Description "Suspicious DLL loaded into Roblox" -Severity $severity -Details $details
                
                $global:BaselineModules[$module] = $true
            }
        }
        
        # Check for module removal (unusual)
        $removedModules = @()
        foreach ($module in $global:BaselineModules.Keys) {
            if ($module -notin $modules) {
                $removedModules += $module
            }
        }
        
        if ($removedModules.Count -gt 0) {
            Write-Host "[WARNING] Modules unloaded: $($removedModules -join ', ')" -ForegroundColor Magenta
        }
        
    } catch {
        Write-Host "[ERROR] DLL detection failed: $_" -ForegroundColor Red
    }
}

# ============= MEMORY ANOMALY DETECTION =============
function Detect-MemoryAnomalies {
    if (-not $global:RobloxPID) { return }
    
    try {
        $process = Get-Process -Id $global:RobloxPID -ErrorAction SilentlyContinue
        if (-not $process) { return }
        
        # Use Windows API to scan memory regions
        Add-Type -TypeDefinition @"
        using System;
        using System.Diagnostics;
        using System.Runtime.InteropServices;

        public class MemoryScan {
            [DllImport("kernel32.dll", SetLastError = true)]
            public static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, uint dwProcessId);
            
            [DllImport("kernel32.dll", SetLastError = true)]
            public static extern bool VirtualQueryEx(IntPtr hProcess, IntPtr lpAddress, out MEMORY_BASIC_INFORMATION lpBuffer, uint dwLength);
            
            [DllImport("kernel32.dll", SetLastError = true)]
            public static extern bool CloseHandle(IntPtr hObject);
            
            [Flags]
            public enum PageProtect : uint {
                PAGE_NOACCESS = 0x01,
                PAGE_READONLY = 0x02,
                PAGE_READWRITE = 0x04,
                PAGE_WRITECOPY = 0x08,
                PAGE_EXECUTE = 0x10,
                PAGE_EXECUTE_READ = 0x20,
                PAGE_EXECUTE_READWRITE = 0x40,
                PAGE_EXECUTE_WRITECOPY = 0x80
            }
            
            [StructLayout(LayoutKind.Sequential)]
            public struct MEMORY_BASIC_INFORMATION {
                public IntPtr BaseAddress;
                public IntPtr AllocationBase;
                public PageProtect AllocationProtect;
                public IntPtr RegionSize;
                public PageProtect Protect;
                public uint Type;
            }
            
            public static int CountSuspiciousMemory(uint pid) {
                IntPtr hProcess = OpenProcess(0x0400, false, pid);
                if (hProcess == IntPtr.Zero) return -1;
                
                int suspiciousCount = 0;
                IntPtr address = IntPtr.Zero;
                
                try {
                    while (address.ToInt64() < long.MaxValue / 2) {
                        MEMORY_BASIC_INFORMATION mbi;
                        if (!VirtualQueryEx(hProcess, address, out mbi, (uint)Marshal.SizeOf(typeof(MEMORY_BASIC_INFORMATION)))) {
                            break;
                        }
                        
                        // Look for RWX (Execute + ReadWrite = suspicious)
                        if ((mbi.Protect == PageProtect.PAGE_EXECUTE_READWRITE) && (mbi.Type == 0x20000)) {
                            suspiciousCount++;
                        }
                        
                        address = new IntPtr(address.ToInt64() + mbi.RegionSize.ToInt64());
                    }
                } finally {
                    CloseHandle(hProcess);
                }
                
                return suspiciousCount;
            }
        }
"@ -ErrorAction SilentlyContinue

        $suspiciousMemory = [MemoryScan]::CountSuspiciousMemory($global:RobloxPID)
        
        if ($suspiciousMemory -gt 5) {
            Write-Host "[MEMORY] Suspicious RWX regions detected: $suspiciousMemory regions" -ForegroundColor Red
            Send-DiscordAlert -Title "Memory Anomaly Detected" -Description "Multiple RWX (Execute+Write) memory regions found" -Severity "HIGH" -Details @{
                "Suspicious Regions" = $suspiciousMemory
                "Indicator" = "Possible shellcode injection"
            }
        }
        
    } catch {
        Write-Host "[WARNING] Memory scan limited: $_" -ForegroundColor Yellow
    }
}

# ============= THREAD MONITORING =============
function Detect-SuspiciousThreads {
    if (-not $global:RobloxPID) { return }
    
    try {
        $process = Get-Process -Id $global:RobloxPID -ErrorAction SilentlyContinue
        if (-not $process) { return }
        
        $threads = @(Get-Process -Id $global:RobloxPID -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Threads)
        
        if ($global:BaselineThreads.Count -eq 0) {
            foreach ($thread in $threads) {
                $global:BaselineThreads[$thread.Id] = $thread.StartTime
            }
            Write-Host "[THREAD_MONITOR] Baseline: $($threads.Count) threads" -ForegroundColor Green
            return
        }
        
        # Detect new threads
        $newThreads = @()
        foreach ($thread in $threads) {
            if (-not $global:BaselineThreads.ContainsKey($thread.Id)) {
                $newThreads += $thread
                $global:BaselineThreads[$thread.Id] = $thread.StartTime
                
                Write-Host "[THREAD] New thread created: ID $($thread.Id)" -ForegroundColor Yellow
            }
        }
        
        if ($newThreads.Count -gt 3) {
            Write-Host "[THREAD] Suspicious: $($newThreads.Count) new threads in single check" -ForegroundColor Red
            Send-DiscordAlert -Title "Suspicious Thread Activity" -Description "Multiple threads created rapidly" -Severity "MEDIUM" -Details @{
                "New Threads" = $newThreads.Count
                "Total Threads" = $threads.Count
            }
        }
        
    } catch {
        Write-Host "[WARNING] Thread detection failed: $_" -ForegroundColor Yellow
    }
}

# ============= PROCESS ACCESS MONITORING =============
function Detect-ProcessAccess {
    try {
        # Find all processes trying to access Roblox
        $allProcesses = Get-Process | Where-Object { $_.Id -ne $global:RobloxPID -and $_.Id -ne $PID }
        $suspiciousProcesses = @()
        
        foreach ($proc in $allProcesses) {
            # Exclude system and known safe processes
            $safeProcNames = @("explorer", "svchost", "dwm", "winlogon", "lsass", "csrss", "conhost", "OneDrive", "chrome", "firefox")
            
            if ($proc.ProcessName -in $safeProcNames) {
                continue
            }
            
            # Check for known cheat/injection tools patterns
            if ($proc.ProcessName -match "(cheat|inject|hack|mod|exploit|debug)" -or $proc.ProcessName.Length -lt 3) {
                $suspiciousProcesses += $proc
            }
        }
        
        if ($suspiciousProcesses.Count -gt 0) {
            foreach ($proc in $suspiciousProcesses) {
                Write-Host "[ACCESS] Suspicious process detected: $($proc.ProcessName) (PID: $($proc.Id))" -ForegroundColor Red
                Send-DiscordAlert -Title "Suspicious Process Detected" -Description "Process with cheat/injection indicators found" -Severity "MEDIUM" -Details @{
                    "Process Name" = $proc.ProcessName
                    "Process ID" = $proc.Id
                    "Memory Usage" = "$([Math]::Round($proc.WorkingSet / 1MB, 2)) MB"
                }
            }
        }
        
    } catch {
        Write-Host "[WARNING] Process access detection failed: $_" -ForegroundColor Yellow
    }
}

# ============= UTILITY FUNCTIONS =============
function Test-StandardDLL {
    param([string]$DLLPath)
    
    $standardPaths = @(
        "C:\Windows\System32",
        "C:\Windows\SysWOW64",
        "C:\Program Files\Roblox",
        "C:\Program Files (x86)\Roblox"
    )
    
    foreach ($path in $standardPaths) {
        if ($DLLPath -like "$path*") {
            return $true
        }
    }
    
    return $false
}

function Write-StatusBar {
    $timestamp = Get-Date -Format "HH:mm:ss"
    $robloxStatus = if ($global:RobloxPID) { "RUNNING (PID: $global:RobloxPID)" } else { "IDLE" }
    $moduleCount = $global:BaselineModules.Count
    $threadCount = $global:BaselineThreads.Count
    
    Write-Host "[$timestamp] Roblox: $robloxStatus | Modules: $moduleCount | Threads: $threadCount" -ForegroundColor Cyan
}

# ============= MAIN MONITORING LOOP =============
function Start-Monitoring {
    Write-Host "╔════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║  MalkaviaGuard v1.0 - Anti-Cheat      ║" -ForegroundColor Magenta
    Write-Host "║  Monitoring: RobloxPlayerBeta.exe      ║" -ForegroundColor Magenta
    Write-Host "║  Check Interval: $CheckInterval ms            ║" -ForegroundColor Magenta
    Write-Host "╚════════════════════════════════════════╝" -ForegroundColor Magenta
    
    Write-Host "`n[INIT] Starting monitoring..." -ForegroundColor Green
    Send-DiscordAlert -Title "MalkaviaGuard Started" -Description "Anti-cheat monitoring is now active" -Severity "LOW"
    
    $checkCount = 0
    
    while ($true) {
        try {
            $checkCount++
            Write-StatusBar
            
            if (Find-RobloxProcess) {
                # Run all detection modules
                Detect-DLLInjection
                Detect-MemoryAnomalies
                Detect-SuspiciousThreads
                Detect-ProcessAccess
            } else {
                # Roblox not running - show idle state every 30 checks
                if ($checkCount % 30 -eq 0) {
                    Write-Host "[INFO] Waiting for Roblox process..." -ForegroundColor Gray
                }
            }
            
            Start-Sleep -Milliseconds $CheckInterval
            
        } catch {
            Write-Host "[CRITICAL ERROR] $_" -ForegroundColor Red
            Start-Sleep -Seconds 5
        }
    }
}

# ============= ENTRY POINT =============
if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "[ERROR] This script requires administrator privileges!" -ForegroundColor Red
    Write-Host "[INFO] Re-running as administrator..." -ForegroundColor Yellow
    
    $scriptPath = $MyInvocation.MyCommand.Path
    $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`""
    Start-Process powershell.exe -ArgumentList $arguments -Verb RunAs
    exit
}

Start-Monitoring
