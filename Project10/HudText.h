#pragma once
#include <array>
#include <cstdio>
#include <DxLib.h>

// Keep separate font handles instead of rebuilding the default glyph cache per label.
inline int CachedHudFont(int size) {
    static std::array<int, 81> fonts = [] { std::array<int, 81> f; f.fill(-1); return f; }();
    if (size < 1 || size > 80) size = 20;
    int& handle = fonts[size];
    if (handle < 0) {
        TCHAR name[256] = {};
        int oldSize = 20, thick = 6, type = -1, charset = -1, edge = -1, italic = 0;
        GetFontStateToHandle(name, &oldSize, &thick, GetDefaultFontHandle(), &type, &charset, &edge, &italic);
        handle = CreateFontToHandle(name, size, thick, type, charset, edge, italic);
    }
    return handle;
}
struct HudText {
    int font = CachedHudFont(20);
    void SetSize(int size) { font = CachedHudFont(size); }
    void Draw(int x, int y, const TCHAR* text, unsigned int color) { DrawStringToHandle(x, y, text, color, font); }
    int Width(const TCHAR* text, int length) { return GetDrawStringWidthToHandle(text, length, font); }
    template<class... Args> void Format(int x, int y, unsigned int color, const char* format, Args... args) {
        char buffer[512]; sprintf_s(buffer, format, args...); Draw(x, y, buffer, color);
    }
};
