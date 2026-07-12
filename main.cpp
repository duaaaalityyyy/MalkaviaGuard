#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#pragma comment(lib, "psapi.lib")

// === CONFIG ===
std::string WEBHOOK_URL = "https://canary.discord.com/api/webhooks/1525718562411516016/GvGdbw1xrdL8X9zL9j4UGgqF13lxmuYqd0VkKcPw2VenOzF4f4Lmkp4G6QU9gOciIYuS";
std::string TARGET_PROCESS = "RobloxPlayerBeta.exe";

// Send to Discord
void SendToDiscord(const std::string& message) {
    std::string json = R"({"content": ")" + message + R"("})";
    std::string cmd = "powershell -c \"Invoke-WebRequest -Uri '" + WEBHOOK_URL + "' -Method POST -Body '" + json + "' -ContentType 'application/json'\"";
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

// Main monitoring loop
void MonitorRoblox() {
    while (true) {
        DWORD pid = GetProcessIdByName(TARGET_PROCESS);
        if (pid != 0) {
            SendToDiscord("🔍 **MalkaviaGuard**: Roblox detected (PID: " + std::to_string(pid) + ") - Monitoring for injections...");

            // TODO: Add advanced injection detection here (module enumeration, thread checking, etc.)
            // For now, basic detection + alert on presence
        }
        Sleep(5000); // Check every 5 seconds
    }
}

int main() {
    SendToDiscord("🚀 **MalkaviaGuard** started successfully!");

    MonitorRoblox();

    return 0;
}
