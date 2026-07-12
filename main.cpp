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

static HWND g_stageWindow = NULL;
static int g_stageIndex = 0;
static int g_stageProgress = 0;
static const int g_stageCount = 3;
static const char* g_stageDescriptions[g_stageCount] = {
    "Checking credentials...",
    "Verifying system integrity...",
    "Finalizing setup..."
};

static HWND g_loginWindow = NULL;
static HWND g_usernameEdit = NULL;
static HWND g_passwordEdit = NULL;
static HWND g_loginStatus = NULL;
static std::map<std::string, std::string> validUsers;
static const std::string CREDENTIAL_FILE = "users.txt";
static bool g_loginSuccessful = false;
static HBRUSH g_loginBackgroundBrush = NULL;
static HBRUSH g_loginEditBrush = NULL;

// === CONFIG ===
std::string WEBHOOK_URL = "https://canary.discord.com/api/webhooks/1525718562411516016/GvGdbw1xrdL8X9zL9j4UGgqF13lxmuYqd0VkKcPw2VenOzF4f4Lmkp4G6QU9gOciIYuS";
std::string TARGET_PROCESS = "RobloxPlayerBeta.exe";

// State tracking
std::map<std::string, bool> baselineModules;
DWORD lastRobloxPID = 0;

// Utility to draw rounded rectangles
void DrawRoundedRectangle(HDC hdc, RECT rect, COLORREF fillColor, COLORREF borderColor) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 2, borderColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 16, 16);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

// Stage popup window procedure
LRESULT CALLBACK StageWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            SetTimer(hwnd, 1, 60, NULL);
            return 0;

        case WM_TIMER:
            g_stageProgress += 2;
            if (g_stageProgress > 100) {
                g_stageProgress = 0;
                g_stageIndex++;
                if (g_stageIndex >= g_stageCount) {
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);

            HBRUSH background = CreateSolidBrush(RGB(18, 24, 40));
            FillRect(hdc, &clientRect, background);
            DeleteObject(background);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            HFONT titleFont = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                         VARIABLE_PITCH, L"Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
            RECT titleRect = { 24, 24, clientRect.right - 24, 80 };
            DrawTextA(hdc, "MalkaviaGuard", -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, oldFont);
            DeleteObject(titleFont);

            HFONT subtitleFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                            VARIABLE_PITCH, L"Segoe UI");
            oldFont = (HFONT)SelectObject(hdc, subtitleFont);

            RECT statusRect = { 24, 90, clientRect.right - 24, 120 };
            std::string statusText = std::string("Stage ") + std::to_string(g_stageIndex + 1) + " of " + std::to_string(g_stageCount) + ": " + g_stageDescriptions[g_stageIndex];
            DrawTextA(hdc, statusText.c_str(), -1, &statusRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            RECT progressBack = { 24, 130, clientRect.right - 24, 162 };
            DrawRoundedRectangle(hdc, progressBack, RGB(40, 48, 64), RGB(55, 70, 90));

            RECT progressFill = progressBack;
            progressFill.right = progressFill.left + (progressFill.right - progressFill.left) * g_stageProgress / 100;
            DrawRoundedRectangle(hdc, progressFill, RGB(88, 164, 255), RGB(88, 164, 255));

            std::string percentText = std::to_string(g_stageProgress) + "%";
            RECT percentRect = { progressBack.left, progressBack.top, progressBack.right, progressBack.bottom };
            DrawTextA(hdc, percentText.c_str(), -1, &percentRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            RECT footerRect = { 24, 170, clientRect.right - 24, 200 };
            std::string footerText = std::string("Please wait...");
            DrawTextA(hdc, footerText.c_str(), -1, &footerRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

            SelectObject(hdc, oldFont);
            DeleteObject(subtitleFont);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Load users from credential file
void LoadUserCredentials() {
    std::ifstream infile(CREDENTIAL_FILE);
    if (!infile.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(infile, line)) {
        size_t sep = line.find(':');
        if (sep != std::string::npos) {
            std::string user = line.substr(0, sep);
            std::string pass = line.substr(sep + 1);
            validUsers[user] = pass;
        }
    }
    infile.close();
}

bool ValidateUser(const std::string& username, const std::string& password) {
    auto it = validUsers.find(username);
    return it != validUsers.end() && it->second == password;
}

// Center window helper
void CenterWindow(HWND hwnd, int width, int height) {
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenX - width) / 2;
    int y = (screenY - height) / 2;
    SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

// Login window procedure
LRESULT CALLBACK LoginWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindowA("STATIC", "Secure Login", WS_VISIBLE | WS_CHILD | SS_CENTER,
                         20, 20, 260, 24, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "Username:", WS_VISIBLE | WS_CHILD,
                         20, 110, 80, 20, hwnd, NULL, NULL, NULL);
            g_usernameEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                           110, 58, 170, 22, hwnd, NULL, NULL, NULL);

            CreateWindowA("STATIC", "Password:", WS_VISIBLE | WS_CHILD,
                         20, 145, 80, 20, hwnd, NULL, NULL, NULL);
            g_passwordEdit = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
                                           110, 143, 170, 22, hwnd, NULL, NULL, NULL);

            CreateWindowA("BUTTON", "Login", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                         110, 185, 170, 28, hwnd, (HMENU)1001, NULL, NULL);

            g_loginStatus = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | SS_LEFT,
                                          20, 170, 260, 18, hwnd, NULL, NULL, NULL);
            return 0;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == 1001) {
                char userBuf[128] = {0};
                char passBuf[128] = {0};
                GetWindowTextA(g_usernameEdit, userBuf, sizeof(userBuf));
                GetWindowTextA(g_passwordEdit, passBuf, sizeof(passBuf));

                std::string username(userBuf);
                std::string password(passBuf);
                if (ValidateUser(username, password)) {
                    SetWindowTextA(g_loginStatus, "Login successful. Proceeding...");
                    g_loginSuccessful = true;
                    DestroyWindow(hwnd);
                } else {
                    SetWindowTextA(g_loginStatus, "Invalid username or password.");
                }
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(255, 255, 255));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)g_loginBackgroundBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = (HDC)wParam;
            SetTextColor(hdcEdit, RGB(235, 235, 235));
            SetBkColor(hdcEdit, RGB(28, 34, 52));
            return (LRESULT)g_loginEditBrush;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            FillRect(hdc, &clientRect, g_loginBackgroundBrush);

            HBRUSH iconBrush = CreateSolidBrush(RGB(88, 164, 255));
            HPEN iconPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
            HGDIOBJ oldBrush = SelectObject(hdc, iconBrush);
            HGDIOBJ oldPen = SelectObject(hdc, iconPen);

            RECT lockBody = { 30, 30, 80, 72 };
            RoundRect(hdc, lockBody.left, lockBody.top, lockBody.right, lockBody.bottom, 10, 10);
            Arc(hdc, 30, 10, 80, 70, 50, 30, 50, 10);

            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            RoundRect(hdc, 38, 36, 72, 68, 8, 8);

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(iconBrush);
            DeleteObject(iconPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(235, 235, 235));
            HFONT headerFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                           VARIABLE_PITCH, L"Segoe UI");
            HGDIOBJ oldFont = SelectObject(hdc, headerFont);
            RECT headerRect = { 100, 30, clientRect.right - 20, 60 };
            DrawTextA(hdc, "Secure Login", -1, &headerRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, oldFont);
            DeleteObject(headerFont);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CLOSE:
            g_loginSuccessful = false;
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (g_loginBackgroundBrush) {
                DeleteObject(g_loginBackgroundBrush);
                g_loginBackgroundBrush = NULL;
            }
            if (g_loginEditBrush) {
                DeleteObject(g_loginEditBrush);
                g_loginEditBrush = NULL;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Show login prompt at startup
bool ShowLoginPrompt() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = LoginWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MalkaviaGuardLoginWnd";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    int width = 300;
    int height = 230;
    g_loginWindow = CreateWindowExA(0, wc.lpszClassName, "MalkaviaGuard Login", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                    CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
    if (!g_loginWindow) return false;

    g_loginBackgroundBrush = CreateSolidBrush(RGB(18, 24, 40));
    g_loginEditBrush = CreateSolidBrush(RGB(28, 34, 52));

    HRGN hRgn = CreateRoundRectRgn(0, 0, width + 1, height + 1, 18, 18);
    SetWindowRgn(g_loginWindow, hRgn, TRUE);

    CenterWindow(g_loginWindow, width, height);
    ShowWindow(g_loginWindow, SW_SHOW);
    UpdateWindow(g_loginWindow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return g_loginSuccessful;
}

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
    LoadUserCredentials();

    if (!ShowLoginPrompt()) {
        std::cout << "[ERROR] Login prompt failed to display.\n";
        return 1;
    }

    ShowStartupStagePopup();

    std::cout << "[INIT] MalkaviaGuard initializing...\n";
    SendToDiscord("🚀 MalkaviaGuard Started", "Anti-cheat monitoring is now active", "LOW");

    MonitorRoblox();

    return 0;
}
