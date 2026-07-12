/*
 * ╔════════════════════════════════════════════════════════════════╗
 * ║          MalkaviaGuard - Advanced Anti-Cheat for Roblox       ║
 * ║                                                                ║
 * ║ DETECTION CAPABILITIES:                                       ║
 * ║ ✓ DLL Injection Detection        ✓ Suspicious Thread Activity ║
 * ║ ✓ Memory Anomaly Detection       ✓ Process Access Monitoring  ║
 * ║ ✓ RWX Memory Region Scanning     ✓ Discord Integration        ║
 * ║ ✓ Module Baseline Tracking       ✓ Real-time Alerting         ║
 * ║                                                                ║
 * ║ NOTE: PowerShell version recommended for better compatibility ║
 * ║       Run: powershell -ExecutionPolicy Bypass -File MalkaviaGuard.ps1 ║
 * ╚════════════════════════════════════════════════════════════════╝
 */

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <ctime>

#pragma comment(lib, "psapi.lib")

// === CONFIG ===
std::string WEBHOOK_URL = "https://canary.discord.com/api/webhooks/1525718562411516016/GvGdbw1xrdL8X9zL9j4UGgqF13lxmuYqd0VkKcPw2VenOzF4f4Lmkp4G6QU9gOciIYuS";
std::string TARGET_PROCESS = "RobloxPlayerBeta.exe";

// State tracking
std::map<std::string, bool> baselineModules;
DWORD lastRobloxPID = 0;

// Send to Discord with rich formatting
void SendToDiscord(const std::string& title, const std::string& description, const std::string& severity = "LOW") {
    std::string json = R"({"embeds": [{"title": ")" + title + R"(", "description": ")" + description + R"(", "color": )" + 
                       (severity == "HIGH" ? "16711680" : severity == "MEDIUM" ? "16776960" : "3329330") + 
                       R"(, "footer": {"text": "MalkaviaGuard v1.0"}}]})";
    
    std::string cmd = "powershell -c \"Invoke-WebRequest -Uri '" + WEBHOOK_URL + "' -Method POST -Body '" + json + "' -ContentType 'application/json' -UseBasicParsing\" 2>$null";
    system(cmd.c_str());
}

// Get process ID by name
DWORD GetProcessIdByName(const std::string& name) {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, name.c_str()) == 0) {
                CloseHandle(hSnapshot);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return 0;
}

// Get all loaded modules for a process
std::vector<std::string> GetProcessModules(DWORD pid) {
    std::vector<std::string> modules;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hSnapshot == INVALID_HANDLE_VALUE) return modules;

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(hSnapshot, &me32)) {
        do {
            modules.push_back(me32.szExePath);
        } while (Module32Next(hSnapshot, &me32));
    }
    CloseHandle(hSnapshot);
    return modules;
}

// Detect new/suspicious DLL injections
void DetectDLLInjection(DWORD pid) {
    std::vector<std::string> currentModules = GetProcessModules(pid);
    
    if (baselineModules.empty()) {
        for (const auto& mod : currentModules) {
            baselineModules[mod] = true;
        }
        std::cout << "[DLL] Baseline established: " << currentModules.size() << " modules\n";
        return;
    }

    for (const auto& mod : currentModules) {
        if (baselineModules.find(mod) == baselineModules.end()) {
            std::cout << "[INJECTION] New DLL detected: " << mod << "\n";
            SendToDiscord("🚨 DLL Injection Detected", "Suspicious DLL loaded: " + mod, "HIGH");
            baselineModules[mod] = true;
        }
    }
}

// Count suspicious memory regions
int CountSuspiciousMemory(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return -1;

    int suspiciousCount = 0;
    MEMORY_BASIC_INFORMATION mbi;
    void* address = 0;

    while (VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi))) {
        // Look for RWX (executable + writable = suspicious)
        if ((mbi.Protect & PAGE_EXECUTE_READWRITE) || (mbi.Protect & PAGE_EXECUTE_WRITECOPY)) {
            if (mbi.Type == MEM_PRIVATE) {
                suspiciousCount++;
            }
        }
        address = (void*)((uintptr_t)address + mbi.RegionSize);
    }

    CloseHandle(hProcess);
    return suspiciousCount;
}

// Get thread count for a process
DWORD GetThreadCount(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    THREADENTRY32 te32;
    te32.dwSize = sizeof(THREADENTRY32);
    DWORD count = 0;

    if (Thread32First(hSnapshot, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                count++;
            }
        } while (Thread32Next(hSnapshot, &te32));
    }
    CloseHandle(hSnapshot);
    return count;
}

// Main monitoring loop
void MonitorRoblox() {
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║  MalkaviaGuard v1.0 - Anti-Cheat      ║\n";
    std::cout << "║  Monitoring: RobloxPlayerBeta.exe      ║\n";
    std::cout << "║  Check Interval: 5000 ms              ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";

    while (true) {
        DWORD pid = GetProcessIdByName(TARGET_PROCESS);
        
        if (pid != 0) {
            if (pid != lastRobloxPID) {
                lastRobloxPID = pid;
                baselineModules.clear();
                std::cout << "[ROBLOX] Process started: PID " << pid << "\n";
                SendToDiscord("✅ Roblox Started", "RobloxPlayerBeta.exe (PID: " + std::to_string(pid) + ")", "LOW");
            }

            // Run detection routines
            DetectDLLInjection(pid);
            
            int suspiciousMemory = CountSuspiciousMemory(pid);
            if (suspiciousMemory > 5) {
                std::cout << "[MEMORY] Suspicious RWX regions: " << suspiciousMemory << "\n";
                SendToDiscord("⚠️ Memory Anomaly", "Found " + std::to_string(suspiciousMemory) + " suspicious RWX regions", "HIGH");
            }

            DWORD threadCount = GetThreadCount(pid);
            if (threadCount > 100) {
                std::cout << "[THREADS] Unusual thread count: " << threadCount << "\n";
                SendToDiscord("⚠️ Suspicious Activity", "Unusual thread count: " + std::to_string(threadCount), "MEDIUM");
            }
        } else {
            if (lastRobloxPID != 0) {
                lastRobloxPID = 0;
                baselineModules.clear();
                std::cout << "[INFO] Roblox process terminated\n";
            }
        }

        Sleep(5000); // Check every 5 seconds
    }
}

int main() {
    std::cout << "[INIT] MalkaviaGuard initializing...\n";
    SendToDiscord("🚀 MalkaviaGuard Started", "Anti-cheat monitoring is now active", "LOW");

    MonitorRoblox();

    return 0;
}
