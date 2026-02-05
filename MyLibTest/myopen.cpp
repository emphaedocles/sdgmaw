// (implementation file — update of existing myopen.cpp)
#include "pch.h"
#include "myopen.h"
#include <windows.h>
#include <richedit.h>
#include <string>
#include <atomic>
#include <cctype> // isdigit

#pragma once

#ifdef MYOPEN_EXPORTS
#define MYOPEN_API __declspec(dllexport)
#else
#define MYOPEN_API __declspec(dllimport)
#endif

static std::atomic<HWND> g_hWnd{ nullptr };
static HWND g_hRichEdit = nullptr;
static HFONT g_hFont = nullptr;
static HBRUSH g_hBgBrush = nullptr;
static HANDLE g_hThread = nullptr;
static DWORD g_threadId = 0;
static std::atomic<int> g_maxLines{ 100 }; // keep last N lines (default 100)

// When true the implementation will try to re-attach input and force-focus the RichEdit
// WARNING: this actively steals focus from other apps/windows. Keep false for polite behavior.
static bool g_forceKeepFocus = false;

static const UINT WM_APPEND_TEXT = WM_USER + 1;
static const UINT WM_APPEND_TEXT_COLOR = WM_USER + 2;
static const UINT WM_CLEAR_TEXT = WM_USER + 3;
static const UINT WM_SCROLL_TO_END = WM_USER + 4;

// highlight colors (adjust as desired)
static const COLORREF HIGHLIGHT_COLOR = RGB(64, 64, 64);   // highlight background for trailing space
static const COLORREF CONTROL_BKG_COLOR = RGB(32, 32, 32); // control default background

// track previous highlighted range (UI thread only)
static LONG g_prevHighlightStart = 0;
static int g_prevHighlightLen = 0;

// Helper: convert ANSI (current code page) to wide
static wchar_t* ToWideAlloc(const char* s)
{
    if (!s) return nullptr;
    int len = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
    if (len <= 0) return nullptr;
    wchar_t* out = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s, -1, out, len);
    return out;
}

// Trim the control so only the last g_maxLines lines remain.
// Must be called on UI thread.
static void TrimLinesIfNeeded(HWND hRich)
{
    if (!hRich) return;
    int maxLines = g_maxLines.load();
    if (maxLines <= 0) return;

    // Get total line count
    LRESULT lineCount = SendMessageW(hRich, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= maxLines) return;

    int linesToRemove = (int)(lineCount - maxLines);
    // Find char index of the start of the line at 'linesToRemove' (i.e., remove lines 0..linesToRemove-1)
    LRESULT idx = SendMessageW(hRich, EM_LINEINDEX, (WPARAM)linesToRemove, 0);
    if (idx <= 0) {
        // if idx == 0, everything removed; just clear
        SetWindowTextW(hRich, L"");
        g_prevHighlightStart = 0;
        g_prevHighlightLen = 0;
        return;
    }

    // Remove text from start (0) up to idx
    SendMessageW(hRich, EM_SETSEL, 0, (LPARAM)idx);
    SendMessageW(hRich, EM_REPLACESEL, FALSE, (LPARAM)L"");

    // After deletion, ensure caret is at end and view scrolled
    int len = (int)SendMessageW(hRich, WM_GETTEXTLENGTH, 0, 0);
    SendMessageW(hRich, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hRich, EM_SCROLLCARET, 0, 0);

    // Previous highlight indices may no longer be valid after trimming — reset
    g_prevHighlightStart = 0;
    g_prevHighlightLen = 0;
}

// Restore previous highlighted region to control background and highlight the trailing space of the last line.
// Must be called on UI thread.
static void HighlightTrailingSpace(HWND hRich)
{
    if (!hRich) return;

    //// restore previous highlight (if still valid)
    //int totalLen = (int)SendMessageW(hRich, WM_GETTEXTLENGTH, 0, 0);
    //if (g_prevHighlightLen > 0 && g_prevHighlightStart >= 0 && g_prevHighlightStart <= totalLen) {
    //    LONG prevStart = g_prevHighlightStart;
    //    int prevLen = g_prevHighlightLen;
    //    LONG prevEnd = prevStart + prevLen;
    //    if (prevEnd > totalLen) prevEnd = totalLen;
    //    if (prevEnd > prevStart) {
    //        SendMessageW(hRich, EM_SETSEL, (WPARAM)prevStart, (LPARAM)prevEnd);
    //        // reset backcolor to control background
    //        CHARFORMAT2W cfRestore;
    //        ZeroMemory(&cfRestore, sizeof(cfRestore));
    //        cfRestore.cbSize = sizeof(cfRestore);
    //        cfRestore.dwMask = CFM_BACKCOLOR;
    //        cfRestore.crBackColor = CONTROL_BKG_COLOR;
    //        SendMessageW(hRich, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cfRestore);
    //    }
    //}

    // find last line
    LRESULT lineCount = SendMessageW(hRich, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= 0) {
        g_prevHighlightStart = 0;
        g_prevHighlightLen = 0;
        return;
    }
    int lastLine = (int)lineCount - 1;
    LRESULT startIdx = SendMessageW(hRich, EM_LINEINDEX, (WPARAM)lastLine, 0);
    if (startIdx < 0) {
        g_prevHighlightStart = 0;
        g_prevHighlightLen = 0;
        return;
    }
    LRESULT lineLen = SendMessageW(hRich, EM_LINELENGTH, (WPARAM)startIdx, 0);
    // lineLen is number of characters in the line; we expect we have appended a trailing space,
    // so we will highlight the last character only.
    if (lineLen <= 0) {
        g_prevHighlightStart = 0;
        g_prevHighlightLen = 0;
        return;
    }

    // compute index of trailing space: last character in line
    LONG trailIdx = (LONG)startIdx + (LONG)lineLen - 1;
    if (trailIdx < 0) {
        g_prevHighlightStart = 0;
        g_prevHighlightLen = 0;
        return;
    }

    // select trailing space only
    SendMessageW(hRich, EM_SETSEL, (WPARAM)trailIdx, (LPARAM)(trailIdx + 1));

    //// apply backcolor to the single-character selection
    //CHARFORMAT2W cf;
    //ZeroMemory(&cf, sizeof(cf));
    //cf.cbSize = sizeof(cf);
    //cf.dwMask = CFM_BACKCOLOR;
    //cf.crBackColor = HIGHLIGHT_COLOR;
    //SendMessageW(hRich, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);

    // keep caret visible on that selection
    SendMessageW(hRich, EM_SCROLLCARET, 0, 0);

    // store for next time (single char)
    g_prevHighlightStart = trailIdx;
    g_prevHighlightLen = 1;
}

static void AppendTextToRich(HWND hRich, const wchar_t* wtxt)
{
    if (!hRich || !wtxt) return;
    // Move caret to end
    int textLen = (int)SendMessageW(hRich, WM_GETTEXTLENGTH, 0, 0);
    SendMessageW(hRich, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);
    // Replace selection (insert)
    SendMessageW(hRich, EM_REPLACESEL, FALSE, (LPARAM)wtxt);

    // Append a single trailing space and ensure it has default formatting
    SendMessageW(hRich, EM_REPLACESEL, FALSE, (LPARAM)L" ");

    // Ensure caret is visible (scroll to end)
    SendMessageW(hRich, EM_SCROLLCARET, 0, 0);

    // Trim lines if needed (must run on UI thread)
    TrimLinesIfNeeded(hRich);

    // Highlight trailing space only
    HighlightTrailingSpace(hRich);
}

static void AppendTextToRichWithColor(HWND hRich, const wchar_t* wtxt, COLORREF color)
{
    if (!hRich || !wtxt) return;
    int textLen = (int)SendMessageW(hRich, WM_GETTEXTLENGTH, 0, 0);
    SendMessageW(hRich, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);

    // Apply selection color for inserted text
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hRich, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);

    // Insert colored text
    SendMessageW(hRich, EM_REPLACESEL, FALSE, (LPARAM)wtxt);

    // Restore selection text color to default (white) for subsequent text
    CHARFORMAT2W cfDefault;
    ZeroMemory(&cfDefault, sizeof(cfDefault));
    cfDefault.cbSize = sizeof(cfDefault);
    cfDefault.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    cfDefault.crTextColor = RGB(255, 255, 255);
    cfDefault.crBackColor = CONTROL_BKG_COLOR;
    SendMessageW(hRich, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cfDefault);

    // Append a single trailing space (will have default formatting)
    SendMessageW(hRich, EM_REPLACESEL, FALSE, (LPARAM)L" ");

    // Ensure caret is visible (scroll to end)
    SendMessageW(hRich, EM_SCROLLCARET, 0, 0);

    // Trim lines if needed (must run on UI thread)
    TrimLinesIfNeeded(hRich);

    // Highlight trailing space only
    HighlightTrailingSpace(hRich);
}

// Data block for color posts
struct AppendColorData {
    wchar_t* text;
    COLORREF color;
};

// ----- Window procedure & UI thread (unchanged except uses above helpers) -----
static LRESULT CALLBACK RichWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // Load RichEdit (Msftedit) and create control
        LoadLibraryW(L"Msftedit.dll");
        RECT rc;
        GetClientRect(hwnd, &rc);

        // create the rich edit control sized to client
        g_hRichEdit = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
            0, 0, rc.right - rc.left, rc.bottom - rc.top, hwnd, nullptr, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), nullptr);
        if (g_hRichEdit) {
            // keep selection visually highlighted even when control/window loses focus
            // EM_HIDESELECTION: wParam = FALSE => selection remains visible when control loses focus
            SendMessageW(g_hRichEdit, EM_HIDESELECTION, FALSE, 0);

            // Create 14pt font for the rich edit control
            HDC hdc = GetDC(nullptr);
            int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(nullptr, hdc);
            int lfHeight = -MulDiv(14, dpiY, 72); // 14 pt
            // Use Segoe UI if available, fall back to default GUI font family
            g_hFont = CreateFontW(lfHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH, L"Segoe UI");
            if (!g_hFont) {
                g_hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            }
            SendMessageW(g_hRichEdit, WM_SETFONT, (WPARAM)g_hFont, MAKELPARAM(TRUE, 0));

            // Background color slightly dim
            g_hBgBrush = CreateSolidBrush(CONTROL_BKG_COLOR);
            SendMessageW(g_hRichEdit, EM_SETBKGNDCOLOR, 0, (LPARAM)CONTROL_BKG_COLOR);

            // Default text color = white, default backcolor = control bg
            CHARFORMAT2W cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_BACKCOLOR;
            cf.crTextColor = RGB(255, 255, 255);
            cf.crBackColor = CONTROL_BKG_COLOR;
            SendMessageW(g_hRichEdit, EM_SETCHARFORMAT, (WPARAM)SCF_DEFAULT, (LPARAM)&cf);
            SendMessageW(g_hRichEdit, EM_SETCHARFORMAT, (WPARAM)SCF_SELECTION, (LPARAM)&cf);

            // Set class background brush so control repaints correctly
            SetClassLongPtrW(g_hRichEdit, GCLP_HBRBACKGROUND, (LONG_PTR)g_hBgBrush);
        }
    }
    return 0;
    case WM_SIZE:
        if (g_hRichEdit) {
            MoveWindow(g_hRichEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        }
        return 0;
    case WM_APPEND_TEXT:
    {
        wchar_t* wtxt = reinterpret_cast<wchar_t*>(lParam);
        AppendTextToRich(g_hRichEdit, wtxt);
        delete[] wtxt;
    }
    return 0;
    case WM_APPEND_TEXT_COLOR:
    {
        AppendColorData* data = reinterpret_cast<AppendColorData*>(lParam);
        if (data) {
            AppendTextToRichWithColor(g_hRichEdit, data->text, data->color);
            delete[] data->text;
            delete data;
        }
    }
    return 0;
    case WM_CLEAR_TEXT:
    {
        if (g_hRichEdit) {
            // Clear all text in the control and ensure caret is at start
            SetWindowTextW(g_hRichEdit, L"");
            SendMessageW(g_hRichEdit, EM_SETSEL, 0, 0);
            SendMessageW(g_hRichEdit, EM_SCROLLCARET, 0, 0);
            g_prevHighlightStart = 0;
            g_prevHighlightLen = 0;
        }
    }
    return 0;
    case WM_SCROLL_TO_END:
    {
        if (g_hRichEdit) {
            int len = (int)SendMessageW(g_hRichEdit, WM_GETTEXTLENGTH, 0, 0);
            SendMessageW(g_hRichEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            SendMessageW(g_hRichEdit, EM_SCROLLCARET, 0, 0);
        }
    }
    return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        // cleanup created GDI objects
        if (g_hFont && g_hFont != (HFONT)GetStockObject(DEFAULT_GUI_FONT)) {
            DeleteObject(g_hFont);
            g_hFont = nullptr;
        }
        if (g_hBgBrush) {
            // Reset background brush to default to avoid double free if class reused
            SetClassLongPtrW(g_hRichEdit, GCLP_HBRBACKGROUND, 0);
            DeleteObject(g_hBgBrush);
            g_hBgBrush = nullptr;
        }
        g_hRichEdit = nullptr;
        g_hWnd = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

static DWORD WINAPI UiThreadProc(LPVOID lp)
{
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = RichWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"MyRichWindowClass";
    RegisterClassExW(&wc);

    // width = 600, height = 600 (example); adjust in CreateWindowExW parameters as needed
    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"SDG MAW Overlay",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd) return 0;

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    g_hWnd = hwnd;
    g_threadId = GetCurrentThreadId();

    if (lp) {
        wchar_t** parts = reinterpret_cast<wchar_t**>(lp);
        if (parts[0]) {
            SetWindowTextW(hwnd, parts[0]);
            delete[] parts[0];
        }
        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);
        if (parts[1]) {
            PostMessageW(hwnd, WM_APPEND_TEXT, 0, (LPARAM)parts[1]);
        }
        delete[] parts;
    }
    else {
        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

// ----- Public API implementations -----
namespace myopen {

    void OpenMe(const char* text, const char* title)
    {
        HWND cur = g_hWnd.load();
        if (cur) {
            SetWindowPos(cur, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(cur);
            if (text) AddText(text);
            return;
        }

        wchar_t* wtitle = ToWideAlloc(title ? title : "SDG MAW Overlay");
        wchar_t* wtext = ToWideAlloc(text);

        wchar_t** parts = new wchar_t* [2];
        parts[0] = wtitle;
        parts[1] = wtext;

        g_hThread = CreateThread(nullptr, 0, UiThreadProc, parts, 0, nullptr);
        if (!g_hThread) {
            delete[] parts[0];
            delete[] parts[1];
            delete[] parts;
        }
    }

    void AddText(const char* text)
    {
        if (!text) return;
        HWND hwnd = g_hWnd.load();
        if (!hwnd) return;
        wchar_t* wtxt = ToWideAlloc(text);
        if (!wtxt) return;
        PostMessageW(hwnd, WM_APPEND_TEXT, 0, (LPARAM)wtxt);
    }

    void AddTextColor(const char* text, unsigned int color)
    {
        if (!text) return;
        HWND hwnd = g_hWnd.load();
        if (!hwnd) return;

        wchar_t* wtxt = ToWideAlloc(text);
        if (!wtxt) return;

        AppendColorData* data = new AppendColorData();
        data->text = wtxt;
        data->color = (COLORREF)color;

        PostMessageW(hwnd, WM_APPEND_TEXT_COLOR, 0, (LPARAM)data);
    }

    // New: parse embedded color tokens and append segments with parsed colors.
    // Token format: form-feed (0x0C, '\f') followed by EXACTLY 5 decimal digits.
    // The 5-digit decimal is parsed as a 16-bit RGB value (RGB565). It will be expanded to 24-bit color.
    void AddTextColorE(const char* text)
    {
        if (!text) return;

        const char* p = text;
        const char* segStart = p;
        COLORREF currentColor = RGB(255, 255, 255); // default white

        while (*p) {
            if (*p == '\f') { // form-feed token start
                // flush segment before token
                if (p > segStart) {
                    std::string seg(segStart, p - segStart);
                    if (!seg.empty()) AddTextColor(seg.c_str(), currentColor);
                }

                // check that there are exactly 5 digits following
                const char* d = p + 1;
                bool ok = true;
                for (int i = 0; i < 5; ++i) {
                    if (!d[i] || !isdigit(static_cast<unsigned char>(d[i]))) { ok = false; break; }
                }

                if (!ok) {
                    // not a valid token: treat the form-feed as literal text
                    AddTextColor("\f", currentColor);
                    p = p + 1; // advance past the form-feed
                    segStart = p;
                    continue;
                }

                // parse exactly 5 decimal digits into a value (expected 0..65535)
                unsigned int val = 0;
                for (int i = 0; i < 5; ++i) {
                    val = val * 10u + (unsigned int)(d[i] - '0');
                }

                // clamp to 16-bit range
                val &= 0xFFFFu;

                // advance p past token (form-feed + 5 digits)
                p = d + 5;
                segStart = p;

                // Interpret val as 16-bit RGB (RGB565): R=5 bits, G=6 bits, B=5 bits
                unsigned int r5 = (val >> 11) & 0x1F;
                unsigned int g6 = (val >> 5) & 0x3F;
                unsigned int b5 = val & 0x1F;

                // Expand to 8-bit channels by bit replication
                unsigned int r8 = (r5 << 3) | (r5 >> 2); // 5->8
                unsigned int g8 = (g6 << 2) | (g6 >> 4); // 6->8
                unsigned int b8 = (b5 << 3) | (b5 >> 2); // 5->8
				if (r8 < 32 && g8 < 32 && b8 < 32) {
                    // avoid too-dark colors: boost to minimum brightness
                    r8 = 255;// += 32;
                    g8 = 255;// += 32;
                    b8 = 255;// += 32;
                }

                currentColor = RGB(static_cast<BYTE>(r8), static_cast<BYTE>(g8), static_cast<BYTE>(b8));

            }
            else {
                p++;
            }
        }

        // flush final segment
        if (p > segStart) {
            std::string seg(segStart, p - segStart);
            if (!seg.empty()) AddTextColor(seg.c_str(), currentColor);
        }
    }

    void ClearText()
    {
        HWND hwnd = g_hWnd.load();
        if (!hwnd) return;
        PostMessageW(hwnd, WM_CLEAR_TEXT, 0, 0);
    }

    void ScrollToEnd()
    {
        HWND hwnd = g_hWnd.load();
        if (!hwnd) return;

        // If caller is already the UI thread, operate directly.
        if (GetCurrentThreadId() == g_threadId) {
            if (g_hRichEdit) {
                int len = (int)SendMessageW(g_hRichEdit, WM_GETTEXTLENGTH, 0, 0);
                SendMessageW(g_hRichEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                SendMessageW(g_hRichEdit, EM_SCROLLCARET, 0, 0);
            }
            return;
        }

        // Synchronously deliver the scroll request to the UI thread to ensure it
        // is processed after any previously queued PostMessage append operations.
        SendMessageW(hwnd, WM_SCROLL_TO_END, 0, 0);
    }

    void SetMaxLines(int maxLines)
    {
        if (maxLines < 1) maxLines = 1;
        g_maxLines.store(maxLines);

        // If UI exists, trim immediately on UI thread so we don't exceed the new limit.
        HWND hwnd = g_hWnd.load();
        if (!hwnd) return;

        if (GetCurrentThreadId() == g_threadId) {
            TrimLinesIfNeeded(g_hRichEdit);
        }
        else {
            // synchronously ensure trim runs on UI thread
            SendMessageW(hwnd, WM_SCROLL_TO_END, 0, 0);
        }
    }

    void Dispose()
    {
        HWND hwnd = g_hWnd.load();
        if (hwnd) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            if (g_hThread) {
                WaitForSingleObject(g_hThread, 3000);
                CloseHandle(g_hThread);
                g_hThread = nullptr;
            }
        }
    }

} // namespace myopen