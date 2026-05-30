#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <cstring>

// Константы
#define WIDTH         800
#define HEIGHT        600
#define TOOLBAR_HEIGHT 40
#define STATUS_HEIGHT  20

// Инструменты
enum Tool { TOOL_PEN, TOOL_LINE, TOOL_RECT, TOOL_ELLIPSE };
Tool currentTool = TOOL_PEN;
COLORREF currentColor = RGB(0,0,0);
int penSize = 2;

// Рисование мышью
bool isDrawing = false;
POINT startPt, endPt;

// Растровое изображение (двойная буферизация)
HBITMAP hBitmap = NULL;
HDC hMemDC = NULL;

// Для резиновой нити
bool shapeInProgress = false;
POINT shapeStart, shapeEnd;

// Элементы интерфейса
HWND hStatusBar;
HWND hPenBtn, hLineBtn, hRectBtn, hEllipseBtn;
HWND hColorRed, hColorGreen, hColorBlue, hColorBlack, hColorYellow;
HWND hSizeUp, hSizeDown, hSizeText;

// Функции
void InitGraphics(HWND hWnd);
void ClearCanvas(COLORREF color);
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int size);
void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int size);
void DrawEllipse(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int size);
void LoadImageFile(HWND hWnd, const char* filename);
void SaveImageFile(HWND hWnd, const char* filename);
void PrintImage(HWND hWnd);
void RenderTemporaryShape(HDC hdc);
void FinalizeShape();
void UpdateStatusBar(HWND hWnd, int x, int y);
void SetPenSize(int size);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            // Инициализация общих элементов (для trackbar)
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC = ICC_BAR_CLASSES;
            InitCommonControlsEx(&icex);

            InitGraphics(hWnd);
            ClearCanvas(RGB(255,255,255));

            // Создание меню (английский)
            HMENU hMenu = CreateMenu();
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, 1001, "Open BMP...");
            AppendMenu(hFileMenu, MF_STRING, 1002, "Save BMP...");
            AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hFileMenu, MF_STRING, 1003, "Print...");
            AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hFileMenu, MF_STRING, 1004, "Exit");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "File");

            HMENU hToolMenu = CreatePopupMenu();
            AppendMenu(hToolMenu, MF_STRING, 2001, "Pen");
            AppendMenu(hToolMenu, MF_STRING, 2002, "Line");
            AppendMenu(hToolMenu, MF_STRING, 2003, "Rectangle");
            AppendMenu(hToolMenu, MF_STRING, 2004, "Ellipse");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hToolMenu, "Tool");

            HMENU hSizeMenu = CreatePopupMenu();
            AppendMenu(hSizeMenu, MF_STRING, 4001, "1 px");
            AppendMenu(hSizeMenu, MF_STRING, 4002, "2 px");
            AppendMenu(hSizeMenu, MF_STRING, 4003, "3 px");
            AppendMenu(hSizeMenu, MF_STRING, 4004, "4 px");
            AppendMenu(hSizeMenu, MF_STRING, 4005, "5 px");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSizeMenu, "Brush Size");

            HMENU hColorMenu = CreatePopupMenu();
            AppendMenu(hColorMenu, MF_STRING, 3001, "Black");
            AppendMenu(hColorMenu, MF_STRING, 3002, "Red");
            AppendMenu(hColorMenu, MF_STRING, 3003, "Green");
            AppendMenu(hColorMenu, MF_STRING, 3004, "Blue");
            AppendMenu(hColorMenu, MF_STRING, 3005, "Yellow");
            AppendMenu(hColorMenu, MF_STRING, 3006, "More...");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hColorMenu, "Color");

            SetMenu(hWnd, hMenu);

            // Панель инструментов (кнопки)
            hPenBtn    = CreateWindow("BUTTON", "Pen",   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 2, 50, 30, hWnd, (HMENU)2001, NULL, NULL);
            hLineBtn   = CreateWindow("BUTTON", "Line",  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 65, 2, 50, 30, hWnd, (HMENU)2002, NULL, NULL);
            hRectBtn   = CreateWindow("BUTTON", "Rect",  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 120,2, 50, 30, hWnd, (HMENU)2003, NULL, NULL);
            hEllipseBtn= CreateWindow("BUTTON", "Ellipse",WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 175,2, 60, 30, hWnd, (HMENU)2004, NULL, NULL);

            // Цвета
            hColorBlack = CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 250,2, 30,30, hWnd, (HMENU)3001, NULL, NULL);
            hColorRed   = CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 285,2, 30,30, hWnd, (HMENU)3002, NULL, NULL);
            hColorGreen = CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 320,2, 30,30, hWnd, (HMENU)3003, NULL, NULL);
            hColorBlue  = CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 355,2, 30,30, hWnd, (HMENU)3004, NULL, NULL);
            hColorYellow= CreateWindow("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 390,2, 30,30, hWnd, (HMENU)3005, NULL, NULL);

            // Заливка цветом кнопок
            HBRUSH br;
            br = CreateSolidBrush(RGB(0,0,0)); SetClassLongPtr(hColorBlack, GCLP_HBRBACKGROUND, (LONG_PTR)br); InvalidateRect(hColorBlack, NULL, TRUE);
            br = CreateSolidBrush(RGB(255,0,0)); SetClassLongPtr(hColorRed,   GCLP_HBRBACKGROUND, (LONG_PTR)br); InvalidateRect(hColorRed, NULL, TRUE);
            br = CreateSolidBrush(RGB(0,255,0)); SetClassLongPtr(hColorGreen, GCLP_HBRBACKGROUND, (LONG_PTR)br); InvalidateRect(hColorGreen, NULL, TRUE);
            br = CreateSolidBrush(RGB(0,0,255)); SetClassLongPtr(hColorBlue,  GCLP_HBRBACKGROUND, (LONG_PTR)br); InvalidateRect(hColorBlue, NULL, TRUE);
            br = CreateSolidBrush(RGB(255,255,0)); SetClassLongPtr(hColorYellow, GCLP_HBRBACKGROUND, (LONG_PTR)br); InvalidateRect(hColorYellow, NULL, TRUE);

            // Размер кисти
            CreateWindow("STATIC", "Size:", WS_CHILD | WS_VISIBLE, 440, 8, 30, 20, hWnd, NULL, NULL, NULL);
            hSizeDown = CreateWindow("BUTTON", "-", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 475, 2, 25, 25, hWnd, (HMENU)4006, NULL, NULL);
            hSizeText = CreateWindow("STATIC", "2", WS_CHILD | WS_VISIBLE | SS_CENTER, 505, 8, 20, 20, hWnd, NULL, NULL, NULL);
            hSizeUp   = CreateWindow("BUTTON", "+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 530, 2, 25, 25, hWnd, (HMENU)4007, NULL, NULL);

            // Статус-бар
            hStatusBar = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0,0,0,0, hWnd, NULL, NULL, NULL);
            int parts[] = {200, -1};
            SendMessage(hStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
            SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)"Tool: Pen");

            break;
        }
        case WM_SIZE: {
            // Изменяем размер статус-бара
            SendMessage(hStatusBar, WM_SIZE, 0, 0);
            // Перерисовываем холст
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            UpdateStatusBar(hWnd, x, y);
            if(isDrawing) {
                endPt.x = x;
                endPt.y = y;
                shapeEnd = endPt;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            isDrawing = true;
            startPt.x = LOWORD(lParam);
            startPt.y = HIWORD(lParam);
            shapeStart = startPt;
            shapeEnd = startPt;
            shapeInProgress = true;
            SetCapture(hWnd);
            break;
        }
        case WM_LBUTTONUP: {
            if(isDrawing) {
                isDrawing = false;
                shapeEnd = endPt;
                FinalizeShape();
                shapeInProgress = false;
                ReleaseCapture();
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // Копируем растровое изображение в окно, учитывая панель инструментов и статус-бар
            RECT rect;
            GetClientRect(hWnd, &rect);
            int toolbarHeight = TOOLBAR_HEIGHT;
            int statusHeight = STATUS_HEIGHT;
            if(hStatusBar) {
                RECT sbRect;
                GetWindowRect(hStatusBar, &sbRect);
                statusHeight = sbRect.bottom - sbRect.top;
            }
            HDC hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, hBitmap);
            StretchBlt(hdc, 0, toolbarHeight, WIDTH, HEIGHT,
                       hdcMem, 0, 0, WIDTH, HEIGHT, SRCCOPY);
            DeleteDC(hdcMem);
            // Временная фигура (резиновая нить)
            if(shapeInProgress) {
                RenderTemporaryShape(hdc);
            }
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_COMMAND: {
            switch(LOWORD(wParam)) {
                case 1001: { // Open
                    OPENFILENAME ofn = {0};
                    char file[MAX_PATH] = "";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = "BMP Files\0*.BMP\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                    if(GetOpenFileName(&ofn)) LoadImageFile(hWnd, file);
                    break;
                }
                case 1002: { // Save
                    OPENFILENAME ofn = {0};
                    char file[MAX_PATH] = "";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = "BMP Files\0*.BMP\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_OVERWRITEPROMPT;
                    if(GetSaveFileName(&ofn)) SaveImageFile(hWnd, file);
                    break;
                }
                case 1003: PrintImage(hWnd); break;
                case 1004: PostQuitMessage(0); break;
                case 2001: currentTool = TOOL_PEN; SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)"Tool: Pen"); break;
                case 2002: currentTool = TOOL_LINE; SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)"Tool: Line"); break;
                case 2003: currentTool = TOOL_RECT; SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)"Tool: Rectangle"); break;
                case 2004: currentTool = TOOL_ELLIPSE; SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)"Tool: Ellipse"); break;
                case 3001: currentColor = RGB(0,0,0); break;
                case 3002: currentColor = RGB(255,0,0); break;
                case 3003: currentColor = RGB(0,255,0); break;
                case 3004: currentColor = RGB(0,0,255); break;
                case 3005: currentColor = RGB(255,255,0); break;
                case 3006: {
                    CHOOSECOLOR cc = {0};
                    COLORREF custom[16] = {0};
                    cc.lStructSize = sizeof(cc);
                    cc.hwndOwner = hWnd;
                    cc.rgbResult = currentColor;
                    cc.lpCustColors = custom;
                    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
                    if(ChooseColor(&cc)) currentColor = cc.rgbResult;
                    break;
                }
                case 4001: SetPenSize(1); break;
                case 4002: SetPenSize(2); break;
                case 4003: SetPenSize(3); break;
                case 4004: SetPenSize(4); break;
                case 4005: SetPenSize(5); break;
                case 4006: if(penSize > 1) SetPenSize(penSize-1); break;
                case 4007: if(penSize < 20) SetPenSize(penSize+1); break;
            }
            break;
        }
        case WM_DESTROY: {
            if(hMemDC) DeleteDC(hMemDC);
            if(hBitmap) DeleteObject(hBitmap);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ----- Реализация графических функций -----
void InitGraphics(HWND hWnd) {
    HDC hdc = GetDC(hWnd);
    hMemDC = CreateCompatibleDC(hdc);
    hBitmap = CreateCompatibleBitmap(hdc, WIDTH, HEIGHT);
    SelectObject(hMemDC, hBitmap);
    ReleaseDC(hWnd, hdc);
}

void ClearCanvas(COLORREF color) {
    RECT rect = {0, 0, WIDTH, HEIGHT};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hMemDC, &rect, brush);
    DeleteObject(brush);
}

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int size) {
    HPEN pen = CreatePen(PS_SOLID, size, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int size) {
    HPEN pen = CreatePen(PS_SOLID, size, color);
    HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Rectangle(hdc, min(x1,x2), min(y1,y2), max(x1,x2), max(y1,y2));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

void DrawEllipse(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int size) {
    HPEN pen = CreatePen(PS_SOLID, size, color);
    HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, min(x1,x2), min(y1,y2), max(x1,x2), max(y1,y2));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

void LoadImageFile(HWND hWnd, const char* filename) {
    HBITMAP hNew = (HBITMAP)LoadImage(NULL, filename, IMAGE_BITMAP, 0, 0,
                                       LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if(!hNew) {
        MessageBox(hWnd, "Failed to load BMP", "Error", MB_ICONERROR);
        return;
    }
    BITMAP bm;
    GetObject(hNew, sizeof(bm), &bm);
    HDC hdc = GetDC(hWnd);
    HDC hdcTemp = CreateCompatibleDC(hdc);
    SelectObject(hdcTemp, hNew);
    ClearCanvas(RGB(255,255,255));
    StretchBlt(hMemDC, 0, 0, WIDTH, HEIGHT, hdcTemp, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
    DeleteDC(hdcTemp);
    ReleaseDC(hWnd, hdc);
    DeleteObject(hNew);
    InvalidateRect(hWnd, NULL, TRUE);
}

void SaveImageFile(HWND hWnd, const char* filename) {
    BITMAP bm;
    GetObject(hBitmap, sizeof(bm), &bm);
    BITMAPFILEHEADER bf = {0};
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = bm.bmBitsPixel;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = ((bm.bmWidth * bm.bmBitsPixel + 31) / 32) * 4 * bm.bmHeight;
    bf.bfType = 0x4D42;
    bf.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bi.biSizeImage;
    bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        MessageBox(hWnd, "Cannot create file", "Error", MB_ICONERROR);
        return;
    }
    DWORD written;
    WriteFile(hFile, &bf, sizeof(bf), &written, NULL);
    WriteFile(hFile, &bi, sizeof(bi), &written, NULL);
    HDC hdc = GetDC(hWnd);
    HDC hMemDC2 = CreateCompatibleDC(hdc);
    SelectObject(hMemDC2, hBitmap);
    BYTE* bits = new BYTE[bi.biSizeImage];
    GetDIBits(hMemDC2, hBitmap, 0, bm.bmHeight, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    WriteFile(hFile, bits, bi.biSizeImage, &written, NULL);
    delete[] bits;
    DeleteDC(hMemDC2);
    ReleaseDC(hWnd, hdc);
    CloseHandle(hFile);
}

void PrintImage(HWND hWnd) {
    PRINTDLG pd = {0};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = hWnd;
    pd.Flags = PD_RETURNDC;
    if(!PrintDlg(&pd)) return;
    HDC hdcPrn = pd.hDC;
    if(!hdcPrn) return;
    int pageWidth = GetDeviceCaps(hdcPrn, HORZRES);
    int pageHeight = GetDeviceCaps(hdcPrn, VERTRES);
    double scale = min((double)pageWidth/WIDTH, (double)pageHeight/HEIGHT);
    int drawWidth = (int)(WIDTH * scale);
    int drawHeight = (int)(HEIGHT * scale);
    int xOff = (pageWidth - drawWidth)/2;
    int yOff = (pageHeight - drawHeight)/2;
    DOCINFO di = {sizeof(DOCINFO), "Drawing", NULL};
    StartDoc(hdcPrn, &di);
    StartPage(hdcPrn);
    StretchBlt(hdcPrn, xOff, yOff, drawWidth, drawHeight, hMemDC, 0,0, WIDTH,HEIGHT, SRCCOPY);
    EndPage(hdcPrn);
    EndDoc(hdcPrn);
    DeleteDC(hdcPrn);
}

void RenderTemporaryShape(HDC hdc) {
    if(!shapeInProgress) return;
    SetROP2(hdc, R2_NOTXORPEN);
    HPEN pen = CreatePen(PS_SOLID, penSize, RGB(255,255,255));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    switch(currentTool) {
        case TOOL_LINE:
            MoveToEx(hdc, shapeStart.x, shapeStart.y, NULL);
            LineTo(hdc, shapeEnd.x, shapeEnd.y);
            break;
        case TOOL_RECT:
            Rectangle(hdc, min(shapeStart.x,shapeEnd.x), min(shapeStart.y,shapeEnd.y),
                           max(shapeStart.x,shapeEnd.x), max(shapeStart.y,shapeEnd.y));
            break;
        case TOOL_ELLIPSE:
            Ellipse(hdc, min(shapeStart.x,shapeEnd.x), min(shapeStart.y,shapeEnd.y),
                         max(shapeStart.x,shapeEnd.x), max(shapeStart.y,shapeEnd.y));
            break;
        default: break;
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    SetROP2(hdc, R2_COPYPEN);
}

void FinalizeShape() {
    switch(currentTool) {
        case TOOL_PEN:
            DrawLine(hMemDC, startPt.x, startPt.y, endPt.x, endPt.y, currentColor, penSize);
            break;
        case TOOL_LINE:
            DrawLine(hMemDC, shapeStart.x, shapeStart.y, shapeEnd.x, shapeEnd.y, currentColor, penSize);
            break;
        case TOOL_RECT:
            DrawRectangle(hMemDC, shapeStart.x, shapeStart.y, shapeEnd.x, shapeEnd.y, currentColor, penSize);
            break;
        case TOOL_ELLIPSE:
            DrawEllipse(hMemDC, shapeStart.x, shapeStart.y, shapeEnd.x, shapeEnd.y, currentColor, penSize);
            break;
    }
}

void UpdateStatusBar(HWND hWnd, int x, int y) {
    char buf[64];
    sprintf(buf, "Pos: %d, %d", x, y);
    SendMessage(hStatusBar, SB_SETTEXT, 1, (LPARAM)buf);
}

void SetPenSize(int size) {
    penSize = size;
    char buf[8];
    sprintf(buf, "%d", size);
    SetWindowText(hSizeText, buf);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = "SimplePaint";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if(!RegisterClass(&wc)) return 0;

    HWND hWnd = CreateWindow("SimplePaint", "Simple Paint - Draw, Print, Load/Save BMP",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             WIDTH+16, HEIGHT+TOOLBAR_HEIGHT+STATUS_HEIGHT+40,
                             NULL, NULL, hInstance, NULL);
    if(!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
