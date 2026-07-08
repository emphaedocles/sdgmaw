#include "pch.h"
#include "FullScreenOverlay.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <functional>
#include <mutex>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static HWND s_hwnd = nullptr;
static ULONG_PTR s_gdiplusToken = 0;
static int s_gdiplusRefCount = 0;
static BYTE s_alpha = 255; // global window alpha
static std::mutex s_callbackMutex;
static FullScreenOverlay::DrawCallback s_drawCallback = nullptr;

// Messages for thread-safe operations
static const UINT WM_FSO_SET_CALLBACK = WM_USER + 0x200;
static const UINT WM_FSO_SET_ALPHA = WM_USER + 0x201;

// Helper: start/stop GDI+
static void EnsureGdiplusStart()
{
    if (s_gdiplusRefCount++ == 0)
    {
        GdiplusStartupInput input;
        GdiplusStartup(&s_gdiplusToken, &input, nullptr);
    }
}

static void EnsureGdiplusStop()
{
    if (--s_gdiplusRefCount == 0)
    {
        if (s_gdiplusToken)
        {
            GdiplusShutdown(s_gdiplusToken);
            s_gdiplusToken = 0;
        }
    }
}

// Window procedure
static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        s_hwnd = hwnd;
        return 0;

    case WM_ERASEBKGND:
        return 1; // we handle background in WM_PAINT

    case WM_FSO_SET_CALLBACK:
    {
        // lParam is a pointer to a new callback (allocated with new)
        auto* newCb = reinterpret_cast<FullScreenOverlay::DrawCallback*>(lParam);
        {
            std::lock_guard<std::mutex> lock(s_callbackMutex);
            if (newCb)
            {
                s_drawCallback = *newCb;
                delete newCb;
            }
            else
            {
                s_drawCallback = nullptr;
            }
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_FSO_SET_ALPHA:
    {
        BYTE alpha = (BYTE)wParam;
        s_alpha = alpha;
        SetLayeredWindowAttributes(hwnd, 0, s_alpha, LWA_ALPHA);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        if (width > 0 && height > 0 && s_gdiplusToken)
        {
            // Offscreen bitmap for double-buffered rendering
            Bitmap bmp(width, height, PixelFormat32bppPARGB);
            Graphics g(&bmp);
            g.SetSmoothingMode(SmoothingModeHighQuality);

            // Clear to fully transparent (important for layered window per-pixel alpha)
            g.Clear(Color(0, 0, 0, 0));

            // Invoke user draw callback (if set)
            {
                std::lock_guard<std::mutex> lock(s_callbackMutex);
                if (s_drawCallback)
                {
                    s_drawCallback(g, width, height);
                }
            }

            // Blit to screen
            Graphics screenG(hdc);
            screenG.SetSmoothingMode(SmoothingModeHighQuality);
            screenG.DrawImage(&bmp, 0, 0, width, height);
        }
        else
        {
            // Fallback: simple clear
            HBRUSH br = (HBRUSH)GetStockObject(BLACK_BRUSH);
            FillRect(hdc, &rc, br);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_hwnd = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_callbackMutex);
            s_drawCallback = nullptr;
        }
        EnsureGdiplusStop();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// UI thread proc
static DWORD WINAPI OverlayThreadProc(LPVOID)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FullScreenOverlayClass";
    RegisterClassExW(&wc);

    // Get primary monitor dimensions
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Create full-screen, borderless, topmost, layered, transparent (mouse passthrough) window
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    DWORD style = WS_POPUP | WS_VISIBLE;

    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, L"Overlay",
        style, 0, 0, screenW, screenH, nullptr, nullptr, hInst, nullptr);

    if (!hwnd)
    {
        EnsureGdiplusStop();
        return 0;
    }

    // Set initial alpha
    SetLayeredWindowAttributes(hwnd, 0, s_alpha, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

// Public API -----------------------------------------------------------------
void FullScreenOverlay::Show()
{
    if (s_hwnd) return; // already shown

    EnsureGdiplusStart();

    HANDLE hThread = CreateThread(nullptr, 0, OverlayThreadProc, nullptr, 0, nullptr);
    if (hThread)
    {
        CloseHandle(hThread); // we don't need to track it
    }
    else
    {
        EnsureGdiplusStop();
    }
}

void FullScreenOverlay::Close()
{
    if (!s_hwnd) return;
    PostMessageW(s_hwnd, WM_CLOSE, 0, 0);
}

void FullScreenOverlay::SetDrawCallback(DrawCallback callback)
{
    if (!s_hwnd) return;

    // Allocate callback on heap so we can pass it via PostMessage
    auto* cb = new DrawCallback(callback);
    PostMessageW(s_hwnd, WM_FSO_SET_CALLBACK, 0, (LPARAM)cb);
}

void FullScreenOverlay::Invalidate()
{
    if (s_hwnd) InvalidateRect(s_hwnd, nullptr, FALSE);
}

void FullScreenOverlay::SetAlpha(BYTE alpha)
{
    s_alpha = alpha;
    if (!s_hwnd) return;
    PostMessageW(s_hwnd, WM_FSO_SET_ALPHA, (WPARAM)alpha, 0);
}

HWND FullScreenOverlay::GetHwnd()
{
    return s_hwnd;
}

bool FullScreenOverlay::IsVisible()
{
    if (!s_hwnd) return false;
    return IsWindowVisible(s_hwnd) != FALSE;
}