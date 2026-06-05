#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Structures
struct ClickPoint {
    int x = 0, y = 0;
    std::string label;
    bool enabled = true;
    int clickType = 0; // 0 left, 1 right, 2 middle
};

struct Settings {
    std::vector<ClickPoint> points;
    int cps = 10;
    bool randomDelay = false;
    int randomPercent = 20;
    bool loopMode = true;
};

// Globals
Settings g_settings;
HWND g_hDlg = nullptr;
HWND g_hList = nullptr;
HWND g_hCpsCombo, g_hRandomCheck, g_hRandomSpin, g_hLoopCheck;
HWND g_hStartBtn, g_hStopBtn, g_hAddBtn, g_hRemoveBtn, g_hPickBtn;
HWND g_hStatusStatic, g_hCountStatic, g_hProgress;
HANDLE g_hWorker = nullptr;
volatile bool g_running = false;
volatile bool g_pause = false;
int g_totalClicks = 0;
bool g_isPicking = false;
const UINT HOTKEY_ID = 1;

// Forward declarations
INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
void AddPointToList(int index);
void RefreshList();
void SaveSettings();
void LoadSettings();
void ApplySettingsToUI();
void UpdateStatus(const char* text, bool error = false);
void DoClick(const ClickPoint& pt);
DWORD WINAPI WorkerThread(LPVOID);
void StartClicker();
void StopClicker();
void AddPoint(int x, int y, const char* label, int clickType);
void RemoveSelectedPoint();
void PickPosition();

// Helper: simulate click
void DoClick(const ClickPoint& pt) {
    SetCursorPos(pt.x, pt.y);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    DWORD down = 0, up = 0;
    if (pt.clickType == 0) { down = MOUSEEVENTF_LEFTDOWN; up = MOUSEEVENTF_LEFTUP; }
    else if (pt.clickType == 1) { down = MOUSEEVENTF_RIGHTDOWN; up = MOUSEEVENTF_RIGHTUP; }
    else { down = MOUSEEVENTF_MIDDLEDOWN; up = MOUSEEVENTF_MIDDLEUP; }
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = down;
    inputs[1].type = INPUT_MOUSE; inputs[1].mi.dwFlags = up;
    SendInput(2, inputs, sizeof(INPUT));
}

// Worker thread
DWORD WINAPI WorkerThread(LPVOID) {
    std::random_device rd;
    std::mt19937 gen(rd());
    while (g_running) {
        if (g_pause) { Sleep(50); continue; }
        for (size_t i = 0; i < g_settings.points.size(); ++i) {
            if (!g_settings.points[i].enabled) continue;
            if (!g_running) break;
            DoClick(g_settings.points[i]);
            ++g_totalClicks;
            SetWindowText(g_hCountStatic, std::to_string(g_totalClicks).c_str());
            int progress = (int)((i+1) * 100 / g_settings.points.size());
            SendMessage(g_hProgress, PBM_SETPOS, progress, 0);
            // Highlight current row
            ListView_SetItemState(g_hList, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            int baseDelay = (g_settings.cps > 0) ? (1000 / g_settings.cps) : 100;
            int delay = baseDelay;
            if (g_settings.randomDelay && g_settings.randomPercent > 0) {
                int variation = baseDelay * g_settings.randomPercent / 100;
                std::uniform_int_distribution<int> dist(-variation, variation);
                delay = baseDelay + dist(gen);
                if (delay < 1) delay = 1;
            }
            Sleep(delay);
        }
        if (!g_settings.loopMode) break;
    }
    g_running = false;
    PostMessage(g_hDlg, WM_APP, 0, 0);
    return 0;
}

void StartClicker() {
    if (g_settings.points.empty()) { UpdateStatus("No points added.", true); return; }
    if (g_running) StopClicker();
    g_running = true;
    g_pause = false;
    g_totalClicks = 0;
    SetWindowText(g_hCountStatic, "0");
    SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
    UpdateStatus("Running... Press F12 to stop");
    EnableWindow(g_hAddBtn, FALSE);
    EnableWindow(g_hRemoveBtn, FALSE);
    EnableWindow(g_hPickBtn, FALSE);
    EnableWindow(g_hStartBtn, FALSE);
    EnableWindow(g_hStopBtn, TRUE);
    SetFocus(g_hStopBtn);
    g_hWorker = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
}

void StopClicker() {
    if (!g_running) return;
    g_running = false;
    if (g_hWorker) { WaitForSingleObject(g_hWorker, 2000); CloseHandle(g_hWorker); g_hWorker = nullptr; }
    EnableWindow(g_hAddBtn, TRUE);
    EnableWindow(g_hRemoveBtn, TRUE);
    EnableWindow(g_hPickBtn, TRUE);
    EnableWindow(g_hStartBtn, TRUE);
    EnableWindow(g_hStopBtn, FALSE);
    UpdateStatus("Stopped");
    ListView_SetItemState(g_hList, -1, 0, LVIS_SELECTED);
}

void AddPointToList(int index) {
    const ClickPoint& pt = g_settings.points[index];
    LVITEM lvi = {};
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    char enabledText[4] = "Yes";
    if (!pt.enabled) strcpy(enabledText, "No");
    lvi.pszText = enabledText;
    lvi.lParam = index;
    ListView_InsertItem(g_hList, &lvi);
    ListView_SetItemText(g_hList, index, 1, (char*)pt.label.c_str());
    char buf[32];
    sprintf_s(buf, "%d", pt.x);
    ListView_SetItemText(g_hList, index, 2, buf);
    sprintf_s(buf, "%d", pt.y);
    ListView_SetItemText(g_hList, index, 3, buf);
    const char* types[] = {"Left", "Right", "Middle"};
    ListView_SetItemText(g_hList, index, 4, (char*)types[pt.clickType]);
}

void RefreshList() {
    ListView_DeleteAllItems(g_hList);
    for (size_t i = 0; i < g_settings.points.size(); ++i)
        AddPointToList((int)i);
}

void AddPoint(int x, int y, const char* label, int clickType) {
    ClickPoint pt;
    pt.x = x; pt.y = y;
    pt.label = label ? label : ("Point " + std::to_string(g_settings.points.size()+1));
    pt.enabled = true;
    pt.clickType = clickType;
    g_settings.points.push_back(pt);
    RefreshList();
    SaveSettings();
    UpdateStatus("Point added.");
}

void RemoveSelectedPoint() {
    int sel = ListView_GetNextItem(g_hList, -1, LVNI_SELECTED);
    if (sel >= 0 && sel < (int)g_settings.points.size()) {
        g_settings.points.erase(g_settings.points.begin() + sel);
        RefreshList();
        SaveSettings();
        UpdateStatus("Point removed.");
    }
}

void PickPosition() {
    g_isPicking = true;
    SetCapture(g_hDlg);
    SetCursor(LoadCursor(nullptr, IDC_CROSS));
    UpdateStatus("Move mouse and press LEFT CLICK to capture.");
}

void SaveSettings() {
    std::ofstream f("autoclicker.cfg");
    if (!f) return;
    f << g_settings.cps << "\n";
    f << g_settings.randomDelay << "\n";
    f << g_settings.randomPercent << "\n";
    f << g_settings.loopMode << "\n";
    f << g_settings.points.size() << "\n";
    for (auto& pt : g_settings.points)
        f << pt.x << " " << pt.y << " " << pt.enabled << " " << pt.clickType << " " << pt.label << "\n";
}

void LoadSettings() {
    std::ifstream f("autoclicker.cfg");
    if (!f) return;
    f >> g_settings.cps;
    f >> g_settings.randomDelay;
    f >> g_settings.randomPercent;
    f >> g_settings.loopMode;
    size_t n;
    f >> n;
    g_settings.points.clear();
    for (size_t i = 0; i < n; ++i) {
        ClickPoint pt;
        f >> pt.x >> pt.y >> pt.enabled >> pt.clickType;
        std::getline(f, pt.label);
        if (!pt.label.empty() && pt.label[0] == ' ') pt.label.erase(0,1);
        g_settings.points.push_back(pt);
    }
    ApplySettingsToUI();
    RefreshList();
}

void ApplySettingsToUI() {
    char buf[16];
    sprintf_s(buf, "%d", g_settings.cps);
    SendMessage(g_hCpsCombo, CB_SETCURSEL, SendMessage(g_hCpsCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)buf), 0);
    SendMessage(g_hRandomCheck, BM_SETCHECK, g_settings.randomDelay ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowText(g_hRandomSpin, std::to_string(g_settings.randomPercent).c_str());
    SendMessage(g_hLoopCheck, BM_SETCHECK, g_settings.loopMode ? BST_CHECKED : BST_UNCHECKED, 0);
}

void UpdateStatus(const char* text, bool error) {
    SetWindowText(g_hStatusStatic, text);
}

// Dialog procedure
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_hDlg = hDlg;
            RECT rc;
            GetClientRect(hDlg, &rc);
            g_hList = CreateWindow(WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                   10, 10, rc.right-20, 200, hDlg, nullptr, GetModuleHandle(nullptr), nullptr);
            LVCOLUMN col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 60; col.pszText = "Enabled"; ListView_InsertColumn(g_hList, 0, &col);
            col.cx = 100; col.pszText = "Label"; ListView_InsertColumn(g_hList, 1, &col);
            col.cx = 60; col.pszText = "X"; ListView_InsertColumn(g_hList, 2, &col);
            col.cx = 60; col.pszText = "Y"; ListView_InsertColumn(g_hList, 3, &col);
            col.cx = 80; col.pszText = "Type"; ListView_InsertColumn(g_hList, 4, &col);

            g_hAddBtn = CreateWindow("BUTTON", "Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 220, 60, 25, hDlg, (HMENU)1, nullptr, nullptr);
            g_hRemoveBtn = CreateWindow("BUTTON", "Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 80, 220, 70, 25, hDlg, (HMENU)2, nullptr, nullptr);
            g_hPickBtn = CreateWindow("BUTTON", "Pick Pos", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 160, 220, 80, 25, hDlg, (HMENU)3, nullptr, nullptr);
            CreateWindow("STATIC", "CPS:", WS_CHILD | WS_VISIBLE, 10, 260, 40, 20, hDlg, nullptr, nullptr, nullptr);
            g_hCpsCombo = CreateWindow("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN, 50, 258, 60, 100, hDlg, nullptr, nullptr, nullptr);
            int cps_vals[] = {1,2,5,10,20,50,100,200,500};
            for (int v : cps_vals) { char buf[16]; sprintf_s(buf, "%d", v); SendMessage(g_hCpsCombo, CB_ADDSTRING, 0, (LPARAM)buf); }
            g_hRandomCheck = CreateWindow("BUTTON", "Random Delay", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 130, 260, 100, 20, hDlg, nullptr, nullptr, nullptr);
            CreateWindow("STATIC", "%:", WS_CHILD | WS_VISIBLE, 240, 260, 20, 20, hDlg, nullptr, nullptr, nullptr);
            g_hRandomSpin = CreateWindow("EDIT", "20", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 270, 258, 40, 20, hDlg, nullptr, nullptr, nullptr);
            g_hLoopCheck = CreateWindow("BUTTON", "Loop Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 330, 260, 90, 20, hDlg, nullptr, nullptr, nullptr);
            g_hStartBtn = CreateWindow("BUTTON", "Start (F12)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 300, 100, 30, hDlg, (HMENU)10, nullptr, nullptr);
            g_hStopBtn = CreateWindow("BUTTON", "Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 120, 300, 80, 30, hDlg, (HMENU)11, nullptr, nullptr);
            EnableWindow(g_hStopBtn, FALSE);
            g_hStatusStatic = CreateWindow("STATIC", "Ready. F12 start/stop", WS_CHILD | WS_VISIBLE, 10, 340, 400, 20, hDlg, nullptr, nullptr, nullptr);
            CreateWindow("STATIC", "Clicks:", WS_CHILD | WS_VISIBLE, 10, 370, 50, 20, hDlg, nullptr, nullptr, nullptr);
            g_hCountStatic = CreateWindow("STATIC", "0", WS_CHILD | WS_VISIBLE, 60, 370, 100, 20, hDlg, nullptr, nullptr, nullptr);
            g_hProgress = CreateWindow(PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE, 10, 400, 400, 20, hDlg, nullptr, nullptr, nullptr);
            SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0,100));
            LoadSettings();
            RegisterHotKey(hDlg, HOTKEY_ID, MOD_CONTROL | MOD_ALT, VK_F12);
            return TRUE;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == 1) AddPoint(100, 100, nullptr, 0);
            else if (id == 2) RemoveSelectedPoint();
            else if (id == 3) PickPosition();
            else if (id == 10) {
                // Read settings from UI
                char buf[16];
                int idx = SendMessage(g_hCpsCombo, CB_GETCURSEL, 0, 0);
                SendMessage(g_hCpsCombo, CB_GETLBTEXT, idx, (LPARAM)buf);
                g_settings.cps = atoi(buf);
                g_settings.randomDelay = (SendMessage(g_hRandomCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                GetWindowText(g_hRandomSpin, buf, 16);
                g_settings.randomPercent = atoi(buf);
                g_settings.loopMode = (SendMessage(g_hLoopCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SaveSettings();
                StartClicker();
            } else if (id == 11) StopClicker();
            break;
        }
        case WM_HOTKEY: {
            if (wParam == HOTKEY_ID) {
                if (g_running) StopClicker(); else StartClicker();
            }
            break;
        }
        case WM_APP: StopClicker(); break;
        case WM_LBUTTONDOWN: {
            if (g_isPicking) {
                ReleaseCapture();
                g_isPicking = false;
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                POINT pt;
                GetCursorPos(&pt);
                AddPoint(pt.x, pt.y, nullptr, 0);
                UpdateStatus("Point captured.");
                return TRUE;
            }
            break;
        }
        case WM_CLOSE: {
            StopClicker();
            SaveSettings();
            DestroyWindow(hDlg);
            break;
        }
        case WM_DESTROY: {
            UnregisterHotKey(hDlg, HOTKEY_ID);
            PostQuitMessage(0);
            break;
        }
        default: return FALSE;
    }
    return FALSE;
}

// WinMain
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);
    WNDCLASS wc = {};
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = "AutoClickerClass";
    RegisterClass(&wc);
    HWND hWnd = CreateWindowEx(0, "AutoClickerClass", "AutoClicker Pro", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                               CW_USEDEFAULT, CW_USEDEFAULT, 450, 480, nullptr, nullptr, hInst, nullptr);
    if (!hWnd) return 1;
    ShowWindow(hWnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
