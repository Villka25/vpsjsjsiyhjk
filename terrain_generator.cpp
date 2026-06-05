// main.cpp – AutoClicker Pro (Single File, WinAPI)
// Compile: cl main.cpp /O2 /MT /FeAutoClicker.exe user32.lib gdi32.lib comctl32.lib shell32.lib
// Or with MinGW: g++ main.cpp -O2 -mwindows -static -o AutoClicker.exe -lcomctl32 -lgdi32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// -------------------------------------------------------------
// Structures
// -------------------------------------------------------------
struct ClickPoint {
    int x = 0, y = 0;
    std::string label;
    bool enabled = true;
    int clickType = 0;   // 0=left,1=right,2=middle
};

struct Settings {
    std::vector<ClickPoint> points;
    int cps = 10;
    bool randomDelay = false;
    int randomPercent = 20;
    bool loopMode = true;
};

// -------------------------------------------------------------
// Global Variables
// -------------------------------------------------------------
Settings g_settings;
HWND g_hDlg = nullptr;
HINSTANCE g_hInst = nullptr;
HIMAGELIST g_himl = nullptr;
HWND g_hList = nullptr;
HWND g_hCpsCombo, g_hRandomCheck, g_hRandomSpin, g_hLoopCheck;
HWND g_hStartBtn, g_hStopBtn, g_hAddBtn, g_hRemoveBtn, g_hPickBtn;
HWND g_hStatusStatic, g_hCountStatic, g_hProgress;
HANDLE g_hWorker = nullptr;
DWORD g_workerThreadId = 0;
volatile bool g_running = false;
volatile bool g_pause = false;
int g_totalClicks = 0;
UINT_PTR g_timerId = 0;
bool g_isPicking = false;
POINT g_pickPoint = {0,0};

// Hotkey ID
const UINT HOTKEY_ID = 1;

// -------------------------------------------------------------
// Forward declarations
// -------------------------------------------------------------
LRESULT CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void AddPointToList(const ClickPoint& pt, int index);
void RefreshList();
void SaveSettings();
void LoadSettings();
void ApplySettingsToUI();
void UpdateStatus(const char* text, bool error = false);
void DoClick(const ClickPoint& pt);
DWORD WINAPI WorkerThread(LPVOID lpParam);
void StartClicker();
void StopClicker();
void AddPoint(int x, int y, const char* label, int clickType);
void RemoveSelectedPoint();
void PickPosition();

// -------------------------------------------------------------
// Helper: Simulate mouse click
// -------------------------------------------------------------
void DoClick(const ClickPoint& pt) {
    // Move cursor
    SetCursorPos(pt.x, pt.y);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    DWORD downFlag = 0, upFlag = 0;
    switch (pt.clickType) {
        case 0: downFlag = MOUSEEVENTF_LEFTDOWN; upFlag = MOUSEEVENTF_LEFTUP; break;
        case 1: downFlag = MOUSEEVENTF_RIGHTDOWN; upFlag = MOUSEEVENTF_RIGHTUP; break;
        case 2: downFlag = MOUSEEVENTF_MIDDLEDOWN; upFlag = MOUSEEVENTF_MIDDLEUP; break;
    }
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = downFlag;
    inputs[1].type = INPUT_MOUSE; inputs[1].mi.dwFlags = upFlag;
    SendInput(2, inputs, sizeof(INPUT));
}

// -------------------------------------------------------------
// Worker Thread (actual clicking)
// -------------------------------------------------------------
DWORD WINAPI WorkerThread(LPVOID /*lpParam*/) {
    std::random_device rd;
    std::mt19937 gen(rd());

    while (g_running) {
        if (g_pause) {
            Sleep(50);
            continue;
        }

        // Cycle through enabled points
        for (size_t i = 0; i < g_settings.points.size(); ++i) {
            if (!g_settings.points[i].enabled) continue;
            if (!g_running) break;

            DoClick(g_settings.points[i]);
            g_totalClicks++;
            SetDlgItemInt(g_hDlg, 0, g_totalClicks, FALSE); // using ID 0 as temp

            // Update progress bar and highlight selected row
            int progress = (int)((i+1) * 100 / g_settings.points.size());
            SendMessage(g_hProgress, PBM_SETPOS, progress, 0);
            ListView_SetItemState(g_hList, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(g_hList, i, FALSE);

            // Delay based on CPS
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

        if (!g_settings.loopMode) break; // one cycle only
    }

    g_running = false;
    PostMessage(g_hDlg, WM_APP, 0, 0); // notify stop
    return 0;
}

// -------------------------------------------------------------
// Start / Stop
// -------------------------------------------------------------
void StartClicker() {
    if (g_settings.points.empty()) {
        UpdateStatus("No click points added.", true);
        return;
    }
    if (g_running) StopClicker();

    g_running = true;
    g_pause = false;
    g_totalClicks = 0;
    SetDlgItemText(g_hDlg, 0, "0");
    SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
    UpdateStatus("Running... Press F12 to stop");
    EnableWindow(GetDlgItem(g_hDlg, 1), FALSE); // disable Add
    EnableWindow(GetDlgItem(g_hDlg, 2), FALSE); // disable Remove
    EnableWindow(GetDlgItem(g_hDlg, 3), FALSE); // disable Pick
    EnableWindow(g_hStartBtn, FALSE);
    EnableWindow(g_hStopBtn, TRUE);
    SetFocus(g_hStopBtn);

    g_hWorker = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, &g_workerThreadId);
}

void StopClicker() {
    if (!g_running) return;
    g_running = false;
    if (g_hWorker) {
        WaitForSingleObject(g_hWorker, 2000);
        CloseHandle(g_hWorker);
        g_hWorker = nullptr;
    }
    EnableWindow(GetDlgItem(g_hDlg, 1), TRUE);
    EnableWindow(GetDlgItem(g_hDlg, 2), TRUE);
    EnableWindow(GetDlgItem(g_hDlg, 3), TRUE);
    EnableWindow(g_hStartBtn, TRUE);
    EnableWindow(g_hStopBtn, FALSE);
    UpdateStatus("Stopped.");
    ListView_SetItemState(g_hList, -1, 0, LVIS_SELECTED);
}

// -------------------------------------------------------------
// ListView helpers
// -------------------------------------------------------------
void AddPointToList(const ClickPoint& pt, int index) {
    LVITEM lvi = {};
    lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    lvi.pszText = (char*)(pt.enabled ? "Yes" : "No");
    lvi.iImage = 0;
    lvi.lParam = (LPARAM)(new ClickPoint(pt));
    ListView_InsertItem(g_hList, &lvi);

    std::string lbl = pt.label.empty() ? std::to_string(index+1) : pt.label;
    ListView_SetItemText(g_hList, index, 1, (char*)lbl.c_str());
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
    for (size_t i = 0; i < g_settings.points.size(); ++i) {
        AddPointToList(g_settings.points[i], (int)i);
    }
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
        delete (ClickPoint*)ListView_GetItemParam(g_hList, sel);
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
    UpdateStatus("Move mouse and press LEFT CLICK to capture position.");
}

// -------------------------------------------------------------
// Settings I/O
// -------------------------------------------------------------
void SaveSettings() {
    std::ofstream f("autoclicker.cfg");
    if (!f) return;
    f << g_settings.cps << "\n";
    f << g_settings.randomDelay << "\n";
    f << g_settings.randomPercent << "\n";
    f << g_settings.loopMode << "\n";
    f << g_settings.points.size() << "\n";
    for (auto& pt : g_settings.points) {
        f << pt.x << " " << pt.y << " " << pt.enabled << " " << pt.clickType << " " << pt.label << "\n";
    }
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
    // CPS combobox
    char buf[16];
    sprintf_s(buf, "%d", g_settings.cps);
    ComboBox_SetText(g_hCpsCombo, buf);
    Button_SetCheck(g_hRandomCheck, g_settings.randomDelay ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(g_hDlg, 100, g_settings.randomPercent, FALSE); // ID 100 for spin
    Button_SetCheck(g_hLoopCheck, g_settings.loopMode ? BST_CHECKED : BST_UNCHECKED);
}

void UpdateStatus(const char* text, bool error) {
    SetWindowText(g_hStatusStatic, text);
    if (error) SetWindowText(g_hStatusStatic, text); // could color, but simple
}

// -------------------------------------------------------------
// Dialog Procedure
// -------------------------------------------------------------
INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            g_hDlg = hDlg;
            // Create ListView
            RECT rc;
            GetClientRect(hDlg, &rc);
            g_hList = CreateWindow(WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL,
                                   10, 10, rc.right-20, 200, hDlg, nullptr, g_hInst, nullptr);
            LVCOLUMN col = {};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 60; col.pszText = "Enabled"; ListView_InsertColumn(g_hList, 0, &col);
            col.cx = 100; col.pszText = "Label"; ListView_InsertColumn(g_hList, 1, &col);
            col.cx = 60; col.pszText = "X"; ListView_InsertColumn(g_hList, 2, &col);
            col.cx = 60; col.pszText = "Y"; ListView_InsertColumn(g_hList, 3, &col);
            col.cx = 80; col.pszText = "Click Type"; ListView_InsertColumn(g_hList, 4, &col);

            // Controls
            CreateWindow("BUTTON", "Add Point", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         10, 220, 80, 25, hDlg, (HMENU)1, g_hInst, nullptr);
            CreateWindow("BUTTON", "Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                         100, 220, 80, 25, hDlg, (HMENU)2, g_hInst, nullptr);
            g_hPickBtn = CreateWindow("BUTTON", "Pick Pos", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     190, 220, 80, 25, hDlg, (HMENU)3, g_hInst, nullptr);
            CreateWindow("STATIC", "CPS:", WS_CHILD | WS_VISIBLE,
                         10, 260, 40, 20, hDlg, nullptr, g_hInst, nullptr);
            g_hCpsCombo = CreateWindow("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN,
                                       50, 258, 60, 100, hDlg, nullptr, g_hInst, nullptr);
            for (int cps : {1,2,5,10,20,50,100,200,500}) {
                char buf[16];
                sprintf_s(buf, "%d", cps);
                SendMessage(g_hCpsCombo, CB_ADDSTRING, 0, (LPARAM)buf);
            }
            g_hRandomCheck = CreateWindow("BUTTON", "Random Delay", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                          130, 260, 100, 20, hDlg, nullptr, g_hInst, nullptr);
            CreateWindow("STATIC", "%:", WS_CHILD | WS_VISIBLE,
                         240, 260, 20, 20, hDlg, nullptr, g_hInst, nullptr);
            g_hRandomSpin = CreateWindow("EDIT", "20", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                         270, 258, 40, 20, hDlg, (HMENU)100, g_hInst, nullptr);
            g_hLoopCheck = CreateWindow("BUTTON", "Loop Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                        330, 260, 90, 20, hDlg, nullptr, g_hInst, nullptr);

            g_hStartBtn = CreateWindow("BUTTON", "Start (F12)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       10, 300, 100, 30, hDlg, (HMENU)10, g_hInst, nullptr);
            g_hStopBtn = CreateWindow("BUTTON", "Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      120, 300, 80, 30, hDlg, (HMENU)11, g_hInst, nullptr);
            EnableWindow(g_hStopBtn, FALSE);
            g_hStatusStatic = CreateWindow("STATIC", "Ready. F12 = start/stop", WS_CHILD | WS_VISIBLE,
                                           10, 340, 400, 20, hDlg, nullptr, g_hInst, nullptr);
            CreateWindow("STATIC", "Total clicks:", WS_CHILD | WS_VISIBLE,
                         10, 370, 80, 20, hDlg, nullptr, g_hInst, nullptr);
            g_hCountStatic = CreateWindow("STATIC", "0", WS_CHILD | WS_VISIBLE,
                                          100, 370, 100, 20, hDlg, nullptr, g_hInst, nullptr);
            g_hProgress = CreateWindow(PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE,
                                       10, 400, 400, 20, hDlg, nullptr, g_hInst, nullptr);
            SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0,100));

            LoadSettings();
            RegisterHotKey(hDlg, HOTKEY_ID, MOD_CONTROL | MOD_ALT, VK_F12);
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == 1) { // Add
                AddPoint(100, 100, nullptr, 0);
            } else if (id == 2) { // Remove
                RemoveSelectedPoint();
            } else if (id == 3) { // Pick Pos
                PickPosition();
            } else if (id == 10) { // Start
                // read settings from UI
                char buf[16];
                ComboBox_GetText(g_hCpsCombo, buf, 16);
                g_settings.cps = atoi(buf);
                g_settings.randomDelay = (Button_GetCheck(g_hRandomCheck) == BST_CHECKED);
                g_settings.randomPercent = GetDlgItemInt(hDlg, 100, nullptr, FALSE);
                g_settings.loopMode = (Button_GetCheck(g_hLoopCheck) == BST_CHECKED);
                SaveSettings();
                StartClicker();
            } else if (id == 11) { // Stop
                StopClicker();
            } else if (id == 100) { // random percent edit
                // ignore, read later
            }
            break;
        }

        case WM_HOTKEY: {
            if (wParam == HOTKEY_ID) {
                if (g_running) StopClicker();
                else StartClicker();
            }
            break;
        }

        case WM_APP: { // worker finished
            StopClicker();
            break;
        }

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

// -------------------------------------------------------------
// WinMain Entry Point
// -------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    g_hInst = hInstance;
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);
    DialogBox(hInstance, MAKEINTRESOURCE(NULL), nullptr, DlgProc);
    // Actually we need a dialog template. But we can create via CreateDialog, easier: create window manually.
    // Alternative: use CreateDialog with a resource. But to keep single file, we'll create a main window directly.
    // We'll simulate a dialog by creating a window with class.
    // Simpler: register window class and create main window. But above DlgProc expects dialog resource.
    // Let's fix: Instead of DialogBox, create a window and set DlgProc as its procedure.
    WNDCLASS wc = {};
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = "AutoClickerClass";
    RegisterClass(&wc);
    HWND hWnd = CreateWindowEx(0, "AutoClickerClass", "AutoClicker Pro", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                               CW_USEDEFAULT, CW_USEDEFAULT, 450, 480, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return 1;
    ShowWindow(hWnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
