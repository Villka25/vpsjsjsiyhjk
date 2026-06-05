#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <queue>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// -------------------------------------------------------------
// Глобальные переменные
// -------------------------------------------------------------
HWND g_hDlg = nullptr;
HWND g_hTargetEdit, g_hPpsSpin, g_hCountSpin, g_hStartBtn, g_hStopBtn;
HWND g_hLogList, g_hSentStatic, g_hSuccessStatic, g_hLossStatic, g_hProgress;

std::atomic<bool> g_running{ false };
HANDLE g_hThread = nullptr;
HANDLE g_hIcmp = INVALID_HANDLE_VALUE;

int g_packetsPerSecond = 10;
int g_totalPacketsToSend = 100;
int g_sent = 0, g_success = 0;
std::queue<std::string> g_logQueue;
CRITICAL_SECTION g_logLock;

// -------------------------------------------------------------
// Вспомогательные функции
// -------------------------------------------------------------
void AddLog(const char* text) {
    EnterCriticalSection(&g_logLock);
    g_logQueue.push(text);
    LeaveCriticalSection(&g_logLock);
    PostMessage(g_hDlg, WM_APP, 0, 0);
}

void UpdateStats() {
    char buf[64];
    sprintf_s(buf, "Sent: %d", g_sent);
    SetWindowText(g_hSentStatic, buf);
    sprintf_s(buf, "Success: %d", g_success);
    SetWindowText(g_hSuccessStatic, buf);
    int loss = (g_sent > 0) ? (100 - (g_success * 100 / g_sent)) : 0;
    sprintf_s(buf, "Loss: %d%%", loss);
    SetWindowText(g_hLossStatic, buf);

    if (g_totalPacketsToSend > 0) {
        SendMessage(g_hProgress, PBM_SETPOS, (g_sent * 100) / g_totalPacketsToSend, 0);
    } else {
        SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
    }
}

// -------------------------------------------------------------
// ICMP ping
// -------------------------------------------------------------
DWORD IpStringToAddr(const char* ip) {
    return inet_addr(ip);
}

std::string ResolveDomain(const char* domain) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    ADDRINFOA hints = {}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;
    std::string ip;
    if (getaddrinfo(domain, nullptr, &hints, &result) == 0 && result) {
        sockaddr_in* sockaddr = (sockaddr_in*)result->ai_addr;
        ip = inet_ntoa(sockaddr->sin_addr);
        freeaddrinfo(result);
    }
    WSACleanup();
    return ip;
}

void SendOnePing(const std::string& target) {
    std::string resolvedIp = ResolveDomain(target.c_str());
    if (resolvedIp.empty()) {
        AddLog(("Failed to resolve: " + target).c_str());
        return;
    }

    DWORD ipAddr = IpStringToAddr(resolvedIp.c_str());
    if (ipAddr == INADDR_NONE) {
        AddLog(("Invalid IP: " + resolvedIp).c_str());
        return;
    }

    const DWORD replySize = sizeof(ICMP_ECHO_REPLY) + 32;
    BYTE replyBuffer[replySize];
    ZeroMemory(replyBuffer, replySize);

    DWORD start = GetTickCount();
    DWORD result = IcmpSendEcho(g_hIcmp, ipAddr, nullptr, 0, nullptr, replyBuffer, replySize, 1000);
    DWORD rtt = GetTickCount() - start;
    g_sent++;

    if (result > 0) {
        g_success++;
        char log[256];
        sprintf_s(log, "Reply from %s: time=%dms", resolvedIp.c_str(), rtt);
        AddLog(log);
    } else {
        AddLog(("Timeout / no reply from " + resolvedIp).c_str());
    }
    UpdateStats();
}

// -------------------------------------------------------------
// Рабочий поток (отправка пакетов с заданной скоростью)
// -------------------------------------------------------------
DWORD WINAPI WorkerThread(LPVOID param) {
    std::string target = (char*)param;
    int intervalMs = 1000 / g_packetsPerSecond;
    int sentCount = 0;

    while (g_running) {
        if (g_totalPacketsToSend > 0 && sentCount >= g_totalPacketsToSend) break;

        SendOnePing(target);
        sentCount++;

        // Пауза между пакетами
        if (g_running && intervalMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }

    g_running = false;
    PostMessage(g_hDlg, WM_APP+1, 0, 0);
    return 0;
}

// -------------------------------------------------------------
// Управление тестом
// -------------------------------------------------------------
void StartTest() {
    char target[256];
    GetWindowText(g_hTargetEdit, target, sizeof(target));
    if (strlen(target) == 0) {
        AddLog("Target cannot be empty");
        return;
    }

    if (g_hIcmp == INVALID_HANDLE_VALUE) {
        g_hIcmp = IcmpCreateFile();
        if (g_hIcmp == INVALID_HANDLE_VALUE) {
            AddLog("Failed to create ICMP handle");
            return;
        }
    }

    g_packetsPerSecond = (int)SendMessage(g_hPpsSpin, UDM_GETPOS32, 0, 0);
    g_totalPacketsToSend = (int)SendMessage(g_hCountSpin, UDM_GETPOS32, 0, 0);
    g_sent = 0;
    g_success = 0;
    UpdateStats();
    SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
    AddLog("=== Test started ===");
    AddLog(("Target: " + std::string(target)).c_str());
    AddLog(("PPS: " + std::to_string(g_packetsPerSecond)).c_str());
    AddLog(("Total packets: " + std::string(g_totalPacketsToSend > 0 ? std::to_string(g_totalPacketsToSend) : "unlimited")).c_str());

    g_running = true;
    g_hThread = CreateThread(nullptr, 0, WorkerThread, _strdup(target), 0, nullptr);
    EnableWindow(g_hStartBtn, FALSE);
    EnableWindow(g_hStopBtn, TRUE);
}

void StopTest() {
    g_running = false;
    if (g_hThread) {
        WaitForSingleObject(g_hThread, 2000);
        CloseHandle(g_hThread);
        g_hThread = nullptr;
    }
    EnableWindow(g_hStartBtn, TRUE);
    EnableWindow(g_hStopBtn, FALSE);
    AddLog("=== Test stopped ===");
}

// -------------------------------------------------------------
// Окно и элементы управления
// -------------------------------------------------------------
void AddLogToList(const char* text) {
    int index = ListView_GetItemCount(g_hLogList);
    LVITEM lvi = {};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = index;
    lvi.pszText = (char*)text;
    ListView_InsertItem(g_hLogList, &lvi);
    ListView_EnsureVisible(g_hLogList, index, FALSE);
}

void ClearLog() {
    ListView_DeleteAllItems(g_hLogList);
}

void CreateUI() {
    // Target
    CreateWindow("STATIC", "Target (IP or domain):", WS_CHILD | WS_VISIBLE,
                 10, 10, 150, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    g_hTargetEdit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                 160, 8, 200, 22, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);

    // PPS
    CreateWindow("STATIC", "Packets/sec:", WS_CHILD | WS_VISIBLE,
                 10, 40, 80, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    HWND hPpsEdit = CreateWindow("EDIT", "10", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                 100, 38, 50, 22, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    g_hPpsSpin = CreateWindow(UPDOWN_CLASS, "", WS_CHILD | WS_VISIBLE,
                              150, 38, 15, 22, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(g_hPpsSpin, UDM_SETRANGE32, 1, 1000);
    SendMessage(g_hPpsSpin, UDM_SETBUDDY, (WPARAM)hPpsEdit, 0);

    // Packet count
    CreateWindow("STATIC", "Total packets:", WS_CHILD | WS_VISIBLE,
                 200, 40, 80, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    HWND hCountEdit = CreateWindow("EDIT", "100", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                   290, 38, 60, 22, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    g_hCountSpin = CreateWindow(UPDOWN_CLASS, "", WS_CHILD | WS_VISIBLE,
                                350, 38, 15, 22, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(g_hCountSpin, UDM_SETRANGE32, 0, 1000000);
    SendMessage(g_hCountSpin, UDM_SETBUDDY, (WPARAM)hCountEdit, 0);
    SetWindowText(hCountEdit, "100");

    // Buttons
    g_hStartBtn = CreateWindow("BUTTON", "Start Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               10, 75, 100, 30, g_hDlg, (HMENU)1, GetModuleHandle(nullptr), nullptr);
    g_hStopBtn = CreateWindow("BUTTON", "Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              120, 75, 80, 30, g_hDlg, (HMENU)2, GetModuleHandle(nullptr), nullptr);
    EnableWindow(g_hStopBtn, FALSE);
    CreateWindow("BUTTON", "Clear Log", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                 210, 75, 80, 30, g_hDlg, (HMENU)3, GetModuleHandle(nullptr), nullptr);

    // Statistics
    CreateWindow("STATIC", "Statistics:", WS_CHILD | WS_VISIBLE,
                 10, 120, 60, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    g_hSentStatic = CreateWindow("STATIC", "Sent: 0", WS_CHILD | WS_VISIBLE,
                                 10, 140, 100, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    g_hSuccessStatic = CreateWindow("STATIC", "Success: 0", WS_CHILD | WS_VISIBLE,
                                    120, 140, 100, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    g_hLossStatic = CreateWindow("STATIC", "Loss: 0%", WS_CHILD | WS_VISIBLE,
                                 230, 140, 80, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);

    // Progress bar
    g_hProgress = CreateWindow(PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE,
                               10, 170, 350, 20, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);

    // Log list (virtual list view)
    g_hLogList = CreateWindow(WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_NOCOLUMNHEADER,
                              10, 200, 460, 250, g_hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
    LVCOLUMN col = { LVCF_TEXT | LVCF_WIDTH, 0, 450, "Log" };
    ListView_InsertColumn(g_hLogList, 0, &col);
}

// -------------------------------------------------------------
// Процедура окна
// -------------------------------------------------------------
LRESULT CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_hDlg = hDlg;
            InitializeCriticalSection(&g_logLock);
            CreateUI();
            SetTimer(hDlg, 1, 100, nullptr); // для обновления лога
            return TRUE;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1) StartTest();
            else if (LOWORD(wParam) == 2) StopTest();
            else if (LOWORD(wParam) == 3) ClearLog();
            break;
        }
        case WM_APP: { // добавить строку в лог
            EnterCriticalSection(&g_logLock);
            while (!g_logQueue.empty()) {
                AddLogToList(g_logQueue.front().c_str());
                g_logQueue.pop();
            }
            LeaveCriticalSection(&g_logLock);
            break;
        }
        case WM_APP+1: // тест завершился
            StopTest();
            AddLog("=== Test finished ===");
            break;
        case WM_TIMER:
            if (wParam == 1) {
                // периодическое обновление статистики (на всякий случай)
                UpdateStats();
            }
            break;
        case WM_CLOSE:
            StopTest();
            if (g_hIcmp != INVALID_HANDLE_VALUE) IcmpCloseHandle(g_hIcmp);
            DeleteCriticalSection(&g_logLock);
            DestroyWindow(hDlg);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hDlg, msg, wParam, lParam);
    }
    return 0;
}

// -------------------------------------------------------------
// Точка входа
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_UPDOWN_CLASS | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "LoadTesterClass";
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, "LoadTesterClass", "Network Load Tester (ICMP)",
                               WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                               CW_USEDEFAULT, CW_USEDEFAULT, 500, 500,
                               nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return 1;
    ShowWindow(hWnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
