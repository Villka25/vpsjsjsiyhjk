// Terrain Generator (Perlin Noise Heightmap)
// Compile: g++ -static -O2 -mwindows terrain_generator.cpp -lgdi32 -lcomctl32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <random>
#include <algorithm>

// Константы
#define APP_WIDTH  500
#define APP_HEIGHT 400
#define ID_BTN_GEN  1001
#define ID_WIDTH    1002
#define ID_HEIGHT   1003
#define ID_OCTAVES  1004
#define ID_PERSIST  1005
#define ID_FREQ     1006

// Глобальные переменные
HWND g_hWnd, g_hEditW, g_hEditH, g_hEditOct, g_hEditPers, g_hEditFreq;

// ------------- Шум Перлина (классическая реализация) -------------
class PerlinNoise {
    std::vector<int> p;
public:
    PerlinNoise(unsigned int seed = std::default_random_engine::default_seed) {
        std::mt19937 gen(seed);
        std::uniform_int_distribution<> dis(0, 255);
        p.resize(512);
        for (int i = 0; i < 256; ++i) p[i] = i;
        for (int i = 0; i < 256; ++i) std::swap(p[i], p[dis(gen) % 256]);
        for (int i = 0; i < 256; ++i) p[256 + i] = p[i];
    }
    double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    double lerp(double a, double b, double t) { return a + t * (b - a); }
    double grad(int hash, double x, double y, double z) {
        int h = hash & 15;
        double u = h < 8 ? x : y, v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
    double noise(double x, double y, double z = 0.0) {
        int xi = (int)std::floor(x) & 255;
        int yi = (int)std::floor(y) & 255;
        int zi = (int)std::floor(z) & 255;
        double xf = x - std::floor(x);
        double yf = y - std::floor(y);
        double zf = z - std::floor(z);
        double u = fade(xf);
        double v = fade(yf);
        double w = fade(zf);
        int aaa = p[p[p[xi] + yi] + zi];
        int aba = p[p[p[xi] + yi + 1] + zi];
        int aab = p[p[p[xi] + yi] + zi + 1];
        int abb = p[p[p[xi] + yi + 1] + zi + 1];
        int baa = p[p[p[xi + 1] + yi] + zi];
        int bba = p[p[p[xi + 1] + yi + 1] + zi];
        int bab = p[p[p[xi + 1] + yi] + zi + 1];
        int bbb = p[p[p[xi + 1] + yi + 1] + zi + 1];
        double x1 = lerp(grad(aaa, xf, yf, zf), grad(baa, xf - 1, yf, zf), u);
        double x2 = lerp(grad(aba, xf, yf - 1, zf), grad(bba, xf - 1, yf - 1, zf), u);
        double y1 = lerp(x1, x2, v);
        x1 = lerp(grad(aab, xf, yf, zf - 1), grad(bab, xf - 1, yf, zf - 1), u);
        x2 = lerp(grad(abb, xf, yf - 1, zf - 1), grad(bbb, xf - 1, yf - 1, zf - 1), u);
        double y2 = lerp(x1, x2, v);
        return (lerp(y1, y2, w) + 1.0) / 2.0; // от 0 до 1
    }
};

// Генерация рельефа и сохранение BMP
bool GenerateHeightmap(int width, int height, int octaves, double persistence, double freq, const char* filename) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    PerlinNoise perlin(rand());
    std::vector<unsigned char> pixels(width * height);
    double maxVal = -1e9, minVal = 1e9;
    std::vector<double> vals(width * height);
    // Предварительный расчёт
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double amplitude = 1.0;
            double frequency = freq;
            double noiseVal = 0.0;
            for (int o = 0; o < octaves; ++o) {
                double nx = x * frequency / width;
                double ny = y * frequency / height;
                noiseVal += perlin.noise(nx, ny) * amplitude;
                amplitude *= persistence;
                frequency *= 2.0;
            }
            vals[y * width + x] = noiseVal / (1.0 - (amplitude > 0 ? amplitude : 0)); // нормализация
            if (vals.back() > maxVal) maxVal = vals.back();
            if (vals.back() < minVal) minVal = vals.back();
        }
    }
    // Преобразование в 0-255
    for (size_t i = 0; i < vals.size(); ++i) {
        double norm = (vals[i] - minVal) / (maxVal - minVal);
        pixels[i] = (unsigned char)(norm * 255.0);
    }
    // Запись BMP файла
    BITMAPFILEHEADER bf = {0};
    BITMAPINFOHEADER bi = {0};
    bf.bfType = 0x4D42; // "BM"
    bf.bfSize = sizeof(bf) + sizeof(bi) + width * height;
    bf.bfOffBits = sizeof(bf) + sizeof(bi);
    bi.biSize = sizeof(bi);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 8;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = width * height;
    FILE* f = fopen(filename, "wb");
    if (!f) return false;
    fwrite(&bf, sizeof(bf), 1, f);
    fwrite(&bi, sizeof(bi), 1, f);
    fwrite(pixels.data(), 1, width * height, f);
    fclose(f);
    return true;
}

// ------------------- GUI Window -------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            int y = 20;
            auto AddLabel = [&](const char* text) {
                CreateWindow("STATIC", text, WS_VISIBLE | WS_CHILD, 20, y, 120, 25, hWnd, NULL, NULL, NULL);
            };
            auto AddEdit = [&](int id, int x) {
                return CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, x, y, 80, 25, hWnd, (HMENU)id, NULL, NULL);
            };
            AddLabel("Ширина (пикс):");      g_hEditW   = AddEdit(ID_WIDTH,   150);
            y += 35;
            AddLabel("Высота (пикс):");       g_hEditH   = AddEdit(ID_HEIGHT,  150);
            y += 35;
            AddLabel("Октавы (1-10):");       g_hEditOct = AddEdit(ID_OCTAVES, 150);
            y += 35;
            AddLabel("Persistance (0.1-1):"); g_hEditPers= AddEdit(ID_PERSIST, 150);
            y += 35;
            AddLabel("Частота (0.01-10):");   g_hEditFreq= AddEdit(ID_FREQ,    150);
            y += 45;
            CreateWindow("BUTTON", "Сгенерировать и сохранить", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         50, y, 200, 40, hWnd, (HMENU)ID_BTN_GEN, NULL, NULL);
            // Установка значений по умолчанию
            SetWindowText(g_hEditW,   "512");
            SetWindowText(g_hEditH,   "512");
            SetWindowText(g_hEditOct, "6");
            SetWindowText(g_hEditPers,"0.5");
            SetWindowText(g_hEditFreq,"2.0");
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_GEN) {
                char bufW[16], bufH[16], bufOct[16], bufPers[16], bufFreq[16];
                GetWindowText(g_hEditW, bufW, 16);
                GetWindowText(g_hEditH, bufH, 16);
                GetWindowText(g_hEditOct, bufOct, 16);
                GetWindowText(g_hEditPers, bufPers, 16);
                GetWindowText(g_hEditFreq, bufFreq, 16);
                int w = atoi(bufW);
                int h = atoi(bufH);
                int oct = atoi(bufOct);
                double pers = atof(bufPers);
                double freq = atof(bufFreq);
                if (w < 1 || h < 1 || oct < 1 || oct > 12 || pers < 0.05 || pers > 0.95 || freq < 0.1 || freq > 20) {
                    MessageBox(hWnd, "Некорректные параметры.\nШирина/высота: 1-4096\nОктавы: 1-12\nPersistance: 0.05-0.95\nЧастота: 0.1-20", "Ошибка", MB_OK);
                    break;
                }
                char file[MAX_PATH];
                OPENFILENAME ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = "BMP Files\0*.bmp\0";
                ofn.lpstrFile = file;
                ofn.lpstrFile[0] = 0;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrDefExt = "bmp";
                ofn.Flags = OFN_OVERWRITEPROMPT;
                if (GetSaveFileName(&ofn)) {
                    srand(GetTickCount());
                    if (GenerateHeightmap(w, h, oct, pers, freq, file)) {
                        char msg[256];
                        sprintf(msg, "Рельеф сохранён в %s", file);
                        MessageBox(hWnd, msg, "Успех", MB_OK);
                    } else {
                        MessageBox(hWnd, "Ошибка сохранения файла", "Ошибка", MB_OK);
                    }
                }
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    // Инициализация common controls (не обязательна, но для стиля)
    INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icex);
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "TerrainGenClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClass(&wc);
    g_hWnd = CreateWindow("TerrainGenClass", "Генератор рельефа (Heightmap)", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                          CW_USEDEFAULT, CW_USEDEFAULT, APP_WIDTH, APP_HEIGHT, NULL, NULL, hInst, NULL);
    if (!g_hWnd) return 0;
    ShowWindow(g_hWnd, nShow);
    UpdateWindow(g_hWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
