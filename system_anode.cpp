// SYSTEM ANODE v1.0
// Консольный менеджер 50 утилит для SA-MP
// Компиляция: g++ -static -O2 system_anode.cpp -o system_anode.exe -lgdi32 -luser32

#include <windows.h>
#include <conio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <tlhelp32.h>

using namespace std;

// --------------------------------------------------
//          СТРУКТУРА ДЛЯ ЗАПУЩЕННЫХ ПРОГРАММ
// --------------------------------------------------
struct RunningProgram {
    int id;
    string name;
    PROCESS_INFORMATION pi;
    bool active;
};

vector<RunningProgram> runningProgs;

// --------------------------------------------------
//              ФУНКЦИИ УПРАВЛЕНИЯ КОНСОЛЬЮ
// --------------------------------------------------
void SetFullscreenConsole() {
    HWND console = GetConsoleWindow();
    ShowWindow(console, SW_MAXIMIZE);
    // Убираем полосу прокрутки
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    COORD newSize = { csbi.dwMaximumWindowSize.X, csbi.dwMaximumWindowSize.Y };
    SetConsoleScreenBufferSize(hOut, newSize);
    // Отключаем выделение и быструю правку
    DWORD mode;
    GetConsoleMode(hOut, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    mode &= ~ENABLE_MOUSE_INPUT;
    SetConsoleMode(hOut, mode);
}

void SetConsoleColor(int textColor, int bgColor) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (bgColor << 4) | textColor);
}

void GotoXY(int x, int y) {
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void HideCursor() {
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
}

// --------------------------------------------------
//                 ЛОГО И ЗАГОЛОВОК
// --------------------------------------------------
void PrintLogo() {
    SetConsoleColor(10, 0); // ярко-зелёный
    cout << R"(
   _____               _        __ _   _  ___  ___ 
  / ____|             | |      / _| \ | |/ _ \| __ \
 | (___   _   _ _ __  | |_ ___| |_|  \| | | | | |_| |
  \___ \ | | | | '_ \ | __/ _ \  _| . ` | | | |  _  |
  ____) || |_| | | | || ||  __/ | | |\  | |_| | | \ \
 |_____/  \__, |_| |_| \__\___|_| |_| \_|\___/|_|  \_\
           __/ |                                     
          |___/                                      
    )" << endl;
    SetConsoleColor(7, 0);
    cout << "                     SYSTEM ANODE - Command Center for SA-MP Utilities\n";
    cout << "                     =================================================\n\n";
}

// --------------------------------------------------
//                СПИСОК ПРОГРАММ (50 идей)
// --------------------------------------------------
struct AppInfo {
    int id;
    string name;
    string exePath; // если пусто – заглушка
};

vector<AppInfo> apps = {
    {1,  "SA-MP Server Scanner",        "scanner.exe"},
    {2,  "Auto Updater Launcher",        "launcher_updater.exe"},
    {3,  "Mobile RP Profile Viewer",     ""},
    {4,  "Discord Online Bot",           "discord_bot.exe"},
    {5,  "RCON Telegram Manager",        "rcon_tg.exe"},
    {6,  "Map Converter (MTA->SA-MP)",   ""},
    {7,  "Log Analyzer",                 "log_analyzer.exe"},
    {8,  "Nickname Generator",           ""},
    {9,  "Phone SMS Mass Sender",        ""},
    {10, "Inventory Editor",             ""},
    {11, "Skin Replacer Tool",           "skin_changer.exe"},
    {12, "Ban Tracker",                  ""},
    {13, "Multi-Install Manager",        ""},
    {14, "Discord Market Bot",           "market_bot.exe"},
    {15, "Auto Server Restarter",        "restarter.exe"},
    {16, "Online Pawn Compiler",         ""},
    {17, "Web Server Status",            ""},
    {18, "Radio Mobile App (mock)",      ""},
    {19, "Economy Analyzer",             ""},
    {20, "Dialog Generator",             ""},
    {21, "Enhanced SA-MP Client",        "samp_enhanced.exe"},
    {22, "OBS HUD Plugin",               "obs_hud.dll"},
    {23, "RP Character Social Network",  ""},
    {24, "Currency Exchange",            ""},
    {25, "Mission Editor",               ""},
    {26, "Web Map Viewer",               ""},
    {27, "Ping Checker",                 "ping_tool.exe"},
    {28, "Interior Video Player",        ""},
    {29, "Help Chat Bot",                ""},
    {30, "Database Backup Tool",         "backup.exe"},
    {31, "NPC Name Generator",           ""},
    {32, "Server Voting Tool",           ""},
    {33, "Cache Cleaner",                "cleaner.exe"},
    {34, "Mobile Ticket System",         ""},
    {35, "Weather Sync Bot",             "weather_bot.exe"},
    {36, "Pawn Vulnerability Scanner",   ""},
    {37, "Number Plate Editor",          ""},
    {38, "Mass Mod Installer",           ""},
    {39, "Twitch Donations Integration", ""},
    {40, "Key Configurator",             ""},
    {41, "Telegram Online Monitor",      "telegram_mon.exe"},
    {42, "RP News Generator",            ""},
    {43, "Auto Translator",              ""},
    {44, "Cross-Server Chat Bridge",     "bridge.exe"},
    {45, "Whitelist Checker",            ""},
    {46, "Screenshot Watermarker",       ""},
    {47, "Voice Proximity Plugin",       ""},
    {48, "Online Trade Simulator",       ""},
    {49, "Ban List Manager",             ""},
    {50, "Mobile SA-MP Launcher",        ""}
};

// --------------------------------------------------
//            ЗАПУСК И ОСТАНОВКА ПРОГРАММ
// --------------------------------------------------
void RunProgram(int id) {
    // Проверяем, не запущена ли уже
    for (auto &rp : runningProgs) {
        if (rp.id == id) {
            cout << "[!] Программа с ID " << id << " уже запущена (PID: " << rp.pi.dwProcessId << ").\n";
            return;
        }
    }
    string name;
    string exe = "";
    for (auto &a : apps) {
        if (a.id == id) {
            name = a.name;
            exe = a.exePath;
            break;
        }
    }
    if (exe.empty()) {
        // Заглушка: просто выводим сообщение
        cout << "[SIM] Запуск утилиты \"" << name << "\" (ID " << id << ") - демонстрационный режим.\n";
        // Имитируем "процесс" только в списке (без реального запуска)
        RunningProgram fake;
        fake.id = id;
        fake.name = name;
        fake.active = true;
        fake.pi.hProcess = NULL;
        runningProgs.push_back(fake);
        return;
    }
    // Реальный запуск .exe
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcess(NULL, (LPSTR)exe.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        cout << "[ERR] Не удалось запустить " << exe << ". Код ошибки: " << GetLastError() << "\n";
        return;
    }
    cout << "[OK] Запущена программа \"" << name << "\" (PID: " << pi.dwProcessId << ").\n";
    RunningProgram rp;
    rp.id = id;
    rp.name = name;
    rp.pi = pi;
    rp.active = true;
    runningProgs.push_back(rp);
}

void StopProgram(int id) {
    for (auto it = runningProgs.begin(); it != runningProgs.end(); ++it) {
        if (it->id == id) {
            if (it->pi.hProcess != NULL) {
                // Завершаем процесс
                TerminateProcess(it->pi.hProcess, 0);
                CloseHandle(it->pi.hProcess);
                CloseHandle(it->pi.hThread);
                cout << "[OK] Принудительно завершена программа \"" << it->name << "\" (PID: " << it->pi.dwProcessId << ").\n";
            } else {
                cout << "[OK] Завершена имитация программы \"" << it->name << "\".\n";
            }
            runningProgs.erase(it);
            return;
        }
    }
    cout << "[!] Программа с ID " << id << " не найдена в списке запущенных.\n";
}

void ListRunning() {
    if (runningProgs.empty()) {
        cout << "[INFO] Нет запущенных программ.\n";
        return;
    }
    cout << "\n=== Запущенные программы ===\n";
    for (auto &rp : runningProgs) {
        cout << "ID: " << rp.id << " | " << rp.name;
        if (rp.pi.hProcess != NULL)
            cout << " (PID: " << rp.pi.dwProcessId << ")";
        cout << endl;
    }
    cout << "=============================\n";
}

// --------------------------------------------------
//              ОТОБРАЖЕНИЕ МЕНЮ
// --------------------------------------------------
void PrintMenu(int curPage = 1) {
    const int perPage = 20;
    int total = apps.size();
    int pages = (total + perPage - 1) / perPage;
    if (curPage < 1) curPage = 1;
    if (curPage > pages) curPage = pages;
    int start = (curPage - 1) * perPage;
    int end = start + perPage;
    if (end > total) end = total;
    
    SetConsoleColor(14, 0); // жёлтый
    cout << "\n═══════════════════════════════════════════════════════════════════════════════════\n";
    SetConsoleColor(7, 0);
    cout << "ДОСТУПНЫЕ ПРОГРАММЫ (страница " << curPage << " из " << pages << ")\n";
    cout << "───────────────────────────────────────────────────────────────────────────────────\n";
    for (int i = start; i < end; i++) {
        printf("  %3d  |  %-40s  %s\n", apps[i].id, apps[i].name.c_str(), apps[i].exePath.empty() ? "[SIM]" : "[EXE]");
    }
    cout << "───────────────────────────────────────────────────────────────────────────────────\n";
    SetConsoleColor(10, 0);
    cout << "КОМАНДЫ: run <ID>   stop <ID>   list   page <N>   exit   cls\n";
    SetConsoleColor(7, 0);
    cout << "═══════════════════════════════════════════════════════════════════════════════════\n";
}

// --------------------------------------------------
//                     MAIN
// --------------------------------------------------
int main() {
    SetFullscreenConsole();
    HideCursor();
    SetConsoleColor(7, 0);
    system("cls");
    PrintLogo();

    int currentPage = 1;
    string command;
    PrintMenu(currentPage);

    while (true) {
        cout << "\nANODE> ";
        getline(cin, command);
        if (command == "exit") break;
        else if (command == "cls") { system("cls"); PrintLogo(); PrintMenu(currentPage); continue; }
        else if (command.substr(0,3) == "run") {
            if (command.length() > 4) {
                int id = atoi(command.substr(4).c_str());
                if (id >= 1 && id <= 50) RunProgram(id);
                else cout << "[!] ID должен быть от 1 до 50.\n";
            } else cout << "[!] Использование: run <ID>\n";
        }
        else if (command.substr(0,4) == "stop") {
            if (command.length() > 5) {
                int id = atoi(command.substr(5).c_str());
                StopProgram(id);
            } else cout << "[!] Использование: stop <ID>\n";
        }
        else if (command == "list") ListRunning();
        else if (command.substr(0,4) == "page") {
            if (command.length() > 5) {
                int pg = atoi(command.substr(5).c_str());
                currentPage = pg;
                system("cls");
                PrintLogo();
                PrintMenu(currentPage);
            } else cout << "[!] Использование: page <N>\n";
        }
        else {
            cout << "[!] Неизвестная команда. Доступны: run, stop, list, page, cls, exit\n";
        }
    }

    // Завершаем все запущенные процессы при выходе
    for (auto &rp : runningProgs) {
        if (rp.pi.hProcess != NULL) {
            TerminateProcess(rp.pi.hProcess, 0);
            CloseHandle(rp.pi.hProcess);
            CloseHandle(rp.pi.hThread);
        }
    }
    SetConsoleColor(7, 0);
    system("cls");
    cout << "SYSTEM ANODE завершён.\n";
    return 0;
}
