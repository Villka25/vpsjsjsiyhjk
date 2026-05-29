#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <vector>
#include <cstring>

// Определяем min/max на случай, если их нет (как в MSVC или mingw)
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// Константы
#define WIDTH  800
#define HEIGHT 600

// Инструменты
enum Tool { TOOL_PEN, TOOL_LINE, TOOL_RECT, TOOL_ELLIPSE };
Tool currentTool = TOOL_PEN;

// Цвет и размер кисти
COLORREF currentColor = RGB(0, 0, 0);
int penSize = 2;

// Рисование мышью
bool isDrawing = false;
POINT startPt, endPt;

// Растровое изображение (двойная буферизация)
HBITMAP hBitmap = NULL;
HBITMAP hOldBitmap = NULL;
HDC hMemDC = NULL;

// Обработка временной фигуры
bool shapeInProgress = false;
POINT shapeStart, shapeEnd;

// Функции
void InitGraphics(HWND hWnd);
void ResizeBitmap(HWND hWnd);
void ClearCanvas(COLORREF color);
void DrawPixel(HDC hdc, int x, int y, COLORREF color);
void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void DrawEllipse(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void LoadImageFile(HWND hWnd, const char* filename);
void SaveImageFile(HWND hWnd, const char* filename);
void PrintImage(HWND hWnd);
void RenderTemporaryShape(HDC hdc);
void FinalizeShape();

// Оконная процедура
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            InitGraphics(hWnd);
            ClearCanvas(RGB(255,255,255));
            // Создание меню
            HMENU hMenu = CreateMenu();
            HMENU hFileMenu = CreatePopupMenu();
            AppendMenu(hFileMenu, MF_STRING, 1001, "Открыть BMP...");
            AppendMenu(hFileMenu, MF_STRING, 1002, "Сохранить BMP...");
            AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hFileMenu, MF_STRING, 1003, "Печать...");
            AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hFileMenu, MF_STRING, 1004, "Выход");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "Файл");

            HMENU hToolMenu = CreatePopupMenu();
            AppendMenu(hToolMenu, MF_STRING, 2001, "Перо");
            AppendMenu(hToolMenu, MF_STRING, 2002, "Линия");
            AppendMenu(hToolMenu, MF_STRING, 2003, "Прямоугольник");
            AppendMenu(hToolMenu, MF_STRING, 2004, "Эллипс");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hToolMenu, "Инструмент");

            HMENU hColorMenu = CreatePopupMenu();
            AppendMenu(hColorMenu, MF_STRING, 3001, "Чёрный");
            AppendMenu(hColorMenu, MF_STRING, 3002, "Красный");
            AppendMenu(hColorMenu, MF_STRING, 3003, "Зелёный");
            AppendMenu(hColorMenu, MF_STRING, 3004, "Синий");
            AppendMenu(hColorMenu, MF_STRING, 3005, "Другой...");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hColorMenu, "Цвет");

            SetMenu(hWnd, hMenu);
            break;
        }
        case WM_SIZE: {
            ResizeBitmap(hWnd);
            InvalidateRect(hWnd, NULL, TRUE);
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
        case WM_MOUSEMOVE: {
            if(isDrawing) {
                endPt.x = LOWORD(lParam);
                endPt.y = HIWORD(lParam);
                shapeEnd = endPt;
                InvalidateRect(hWnd, NULL, FALSE);
            }
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
            if(hBitmap) {
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdcMem, hBitmap);
                BITMAP bm;
                GetObject(hBitmap, sizeof(bm), &bm);
                StretchBlt(hdc, 0, 0, WIDTH, HEIGHT,
                           hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                DeleteDC(hdcMem);
            }
            if(shapeInProgress) {
                RenderTemporaryShape(hdc);
            }
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_COMMAND: {
            switch(LOWORD(wParam)) {
                case 1001: {
                    OPENFILENAME ofn = {0};
                    char file[MAX_PATH] = "";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = "BMP Files\0*.BMP\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                    if(GetOpenFileName(&ofn)) {
                        LoadImageFile(hWnd, file);
                    }
                    break;
                }
                case 1002: {
                    OPENFILENAME ofn = {0};
                    char file[MAX_PATH] = "";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hWnd;
                    ofn.lpstrFilter = "BMP Files\0*.BMP\0";
                    ofn.lpstrFile = file;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_OVERWRITEPROMPT;
                    if(GetSaveFileName(&ofn)) {
                        SaveImageFile(hWnd, file);
                    }
                    break;
                }
                case 1003:
                    PrintImage(hWnd);
                    break;
                case 1004:
                    PostQuitMessage(0);
                    break;
                case 2001: currentTool = TOOL_PEN; break;
                case 2002: currentTool = TOOL_LINE; break;
                case 2003: currentTool = TOOL_RECT; break;
                case 2004: currentTool = TOOL_ELLIPSE; break;
                case 3001: currentColor = RGB(0,0,0); break;
                case 3002: currentColor = RGB(255,0,0); break;
                case 3003: currentColor = RGB(0,255,0); break;
                case 3004: currentColor = RGB(0,0,255); break;
                case 3005: {
                    CHOOSECOLOR cc = {0};
                    COLORREF custom[16] = {0};
                    cc.lStructSize = sizeof(cc);
                    cc.hwndOwner = hWnd;
                    cc.rgbResult = currentColor;
                    cc.lpCustColors = custom;
                    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
                    if(ChooseColor(&cc)) {
                        currentColor = cc.rgbResult;
                    }
                    break;
                }
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

void InitGraphics(HWND hWnd) {
    HDC hdc = GetDC(hWnd);
    hMemDC = CreateCompatibleDC(hdc);
    hBitmap = CreateCompatibleBitmap(hdc, WIDTH, HEIGHT);
    hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    ReleaseDC(hWnd, hdc);
}

void ResizeBitmap(HWND hWnd) {
    // Не меняем размер битмапа
}

void ClearCanvas(COLORREF color) {
    RECT rect = {0, 0, WIDTH, HEIGHT};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hMemDC, &rect, brush);
    DeleteObject(brush);
}

void DrawPixel(HDC hdc, int x, int y, COLORREF color) {
    if(x>=0 && x<WIDTH && y>=0 && y<HEIGHT)
        SetPixel(hdc, x, y, color);
}

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, penSize, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawRectangle(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, penSize, color);
    HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Rectangle(hdc, min(x1,x2), min(y1,y2), max(x1,x2), max(y1,y2));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

void DrawEllipse(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, penSize, color);
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
        MessageBox(hWnd, "Не удалось загрузить BMP", "Ошибка", MB_ICONERROR);
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
    DWORD dwTotal = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bi.biSizeImage;
    bf.bfType = 0x4D42;
    bf.bfSize = dwTotal;
    bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        MessageBox(hWnd, "Не удалось создать файл", "Ошибка", MB_ICONERROR);
        return;
    }
    DWORD written;
    WriteFile(hFile, &bf, sizeof(bf), &written, NULL);
    WriteFile(hFile, &bi, sizeof(bi), &written, NULL);
    HDC hdc = GetDC(hWnd);
    HDC hMemDC2 = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC2, hBitmap);
    BYTE* bits = new BYTE[bi.biSizeImage];
    GetDIBits(hMemDC2, hBitmap, 0, bm.bmHeight, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    WriteFile(hFile, bits, bi.biSizeImage, &written, NULL);
    delete[] bits;
    SelectObject(hMemDC2, hOldBmp);
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
    double scaleX = (double)pageWidth / WIDTH;
    double scaleY = (double)pageHeight / HEIGHT;
    double scale = min(scaleX, scaleY);
    int drawWidth = (int)(WIDTH * scale);
    int drawHeight = (int)(HEIGHT * scale);
    int xOff = (pageWidth - drawWidth) / 2;
    int yOff = (pageHeight - drawHeight) / 2;
    DOCINFO di = {sizeof(DOCINFO), "Рисунок", NULL};
    StartDoc(hdcPrn, &di);
    StartPage(hdcPrn);
    StretchBlt(hdcPrn, xOff, yOff, drawWidth, drawHeight,
               hMemDC, 0, 0, WIDTH, HEIGHT, SRCCOPY);
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
            Rectangle(hdc, min(shapeStart.x, shapeEnd.x), min(shapeStart.y, shapeEnd.y),
                           max(shapeStart.x, shapeEnd.x), max(shapeStart.y, shapeEnd.y));
            break;
        case TOOL_ELLIPSE:
            Ellipse(hdc, min(shapeStart.x, shapeEnd.x), min(shapeStart.y, shapeEnd.y),
                         max(shapeStart.x, shapeEnd.x), max(shapeStart.y, shapeEnd.y));
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
            DrawLine(hMemDC, startPt.x, startPt.y, endPt.x, endPt.y, currentColor);
            break;
        case TOOL_LINE:
            DrawLine(hMemDC, shapeStart.x, shapeStart.y, shapeEnd.x, shapeEnd.y, currentColor);
            break;
        case TOOL_RECT:
            DrawRectangle(hMemDC, shapeStart.x, shapeStart.y, shapeEnd.x, shapeEnd.y, currentColor);
            break;
        case TOOL_ELLIPSE:
            DrawEllipse(hMemDC, shapeStart.x, shapeStart.y, shapeEnd.x, shapeEnd.y, currentColor);
            break;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = "SimpleDraw";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if(!RegisterClass(&wc)) return 0;
    HWND hWnd = CreateWindow("SimpleDraw", "Рисовалка + Печать", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, WIDTH+16, HEIGHT+38,
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
