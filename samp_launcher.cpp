#include <windows.h>
#include <string>
#include <sstream>
#include <vector>
#include <winsock2.h>

#pragma comment(lib,"ws2_32.lib")

using namespace std;

vector<string> split(const string &s, char delim) {
    vector<string> result;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) result.push_back(item);
    return result;
}

string GetServerInfo(string ip, int port, int &onlinePlayers, int &maxPlayers, string &hostname, string &gamemode, string &map) {
    WSADATA wsaData;
    SOCKET sock;
    sockaddr_in serverAddr;
    char buffer[2048];
    int bytesReceived;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) return "WSAStartup failed";
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return "Socket creation failed"; }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());

    unsigned char sampPacket[] = { 'S', 'A', 'M', 'P' };
    int ipParts[4];
    sscanf(ip.c_str(), "%d.%d.%d.%d", &ipParts[0], &ipParts[1], &ipParts[2], &ipParts[3]);
    unsigned char queryPacket[] = { 0x69 };

    sendto(sock, (const char*)sampPacket, 4, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    sendto(sock, (const char*)&ipParts, 4, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
    sendto(sock, (const char*)queryPacket, 1, 0, (sockaddr*)&serverAddr, sizeof(serverAddr));

    int timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    bytesReceived = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
    if (bytesReceived == SOCKET_ERROR) { closesocket(sock); WSACleanup(); return "Server offline"; }
    buffer[bytesReceived] = '\0';

    int offset = 9;
    onlinePlayers = buffer[offset++];
    maxPlayers = buffer[offset++];
    while (offset < bytesReceived && buffer[offset] != 0) hostname += buffer[offset++];
    offset++;
    while (offset < bytesReceived && buffer[offset] != 0) gamemode += buffer[offset++];
    offset++;
    while (offset < bytesReceived && buffer[offset] != 0) map += buffer[offset++];
    offset++;

    closesocket(sock);
    WSACleanup();
    return "OK";
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hEditIP, hBtnCheck, hBtnConnect, hStaticResult, hStaticName;
    static string lastIP;

    switch(msg) {
        case WM_CREATE:
            CreateWindow("STATIC", "SA-MP Server IP:", WS_VISIBLE | WS_CHILD, 10, 10, 120, 20, hwnd, NULL, NULL, NULL);
            hEditIP = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 130, 10, 150, 20, hwnd, NULL, NULL, NULL);
            hBtnCheck = CreateWindow("BUTTON", "Check Server", WS_VISIBLE | WS_CHILD, 290, 10, 100, 20, hwnd, (HMENU)1, NULL, NULL);
            hBtnConnect = CreateWindow("BUTTON", "Connect", WS_VISIBLE | WS_CHILD, 400, 10, 80, 20, hwnd, (HMENU)2, NULL, NULL);
            hStaticResult = CreateWindow("STATIC", "Server status: Not checked", WS_VISIBLE | WS_CHILD, 10, 40, 300, 20, hwnd, NULL, NULL, NULL);
            hStaticName = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD, 10, 70, 300, 20, hwnd, NULL, NULL, NULL);
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) {
                char ip[256];
                GetWindowText(hEditIP, ip, 256);
                string ipStr(ip);
                vector<string> parts = split(ipStr, ':');
                if (parts.size() == 2) {
                    string ipAddress = parts[0];
                    int port = stoi(parts[1]);
                    int online, maxp;
                    string hostname, gamemode, mapName;
                    string result = GetServerInfo(ipAddress, port, online, maxp, hostname, gamemode, mapName);
                    if (result == "OK") {
                        char statusText[256];
                        sprintf_s(statusText, "Server status: Online (%d/%d)", online, maxp);
                        SetWindowText(hStaticResult, statusText);
                        char nameText[256];
                        sprintf_s(nameText, "Hostname: %s | Gamemode: %s | Map: %s", hostname.c_str(), gamemode.c_str(), mapName.c_str());
                        SetWindowText(hStaticName, nameText);
                        lastIP = ipStr;
                    } else {
                        SetWindowText(hStaticResult, "Server status: Offline");
                        SetWindowText(hStaticName, "");
                    }
                } else {
                    SetWindowText(hStaticResult, "Invalid format. Use IP:PORT");
                }
            } else if (LOWORD(wParam) == 2) {
                if (lastIP.empty()) { MessageBox(hwnd, "Please check the server status first.", "Error", MB_OK); return 0; }
                SHELLEXECUTEINFO ShExecInfo = {0};
                ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
                ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
                ShExecInfo.lpVerb = "open";
                ShExecInfo.lpFile = "samp.exe";
                string params = lastIP;
                ShExecInfo.lpParameters = params.c_str();
                ShExecInfo.nShow = SW_SHOW;
                ShellExecuteEx(&ShExecInfo);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "SampLauncherClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    HWND hwnd = CreateWindow("SampLauncherClass", "SA-MP Server Browser", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 520, 120, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    WSACleanup();
    return msg.wParam;
}
