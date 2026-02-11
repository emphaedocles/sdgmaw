#include "pch.h"
#include "TestCanvas.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static HWND s_hwnd = nullptr;
static ULONG_PTR s_gdiplusToken = 0;
static int s_gdiplusRefCount = 0;

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

static std::wstring Utf8ToWide(const char* utf8)
{
	if (!utf8) return std::wstring();
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (len == 0) return std::wstring();
	std::wstring out; out.resize(len);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], len);
	if (!out.empty() && out.back() == L'\0') out.pop_back();
	return out;
}

static LRESULT CALLBACK TestCanvas_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		s_hwnd = hwnd;
		return 0;

	case WM_SIZE:
		InvalidateRect(hwnd, nullptr, TRUE);
		return 0;

	case WM_ERASEBKGND:
		// We'll fully paint in WM_PAINT (double-buffer) -> avoid flicker
		return 1;

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
			// offscreen GDI+ bitmap
			Bitmap bmp(width, height, PixelFormat32bppPARGB);
			Graphics g(&bmp);
			g.SetSmoothingMode(SmoothingModeHighQuality);

			// Blank background (dark)
			SolidBrush bg(Color(255, 18, 18, 18));
			g.FillRectangle(&bg, RectF(0, 0, (REAL)width, (REAL)height));

			// Draw a light border and a small "Ready" label so we can visually confirm rendering.
			Pen border(Color(200, 120, 120, 120), 1.0f);
			g.DrawRectangle(&border, 0.5f, 0.5f, (REAL)width - 1.0f, (REAL)height - 1.0f);

			FontFamily ff(L"Segoe UI");
			Font labelFont(&ff, 12.0f, FontStyleRegular, UnitPixel);
			SolidBrush txt(Color(255, 220, 220, 220));
			//std::wstring msg = Utf8ToWide("GDI+ Test Canvas - Blank (draw here)");
			//RectF textRect(10.0f, 10.0f, (REAL)width - 20.0f, 30.0f);
			//g.DrawString(msg.c_str(), -1, &labelFont, textRect, &txt);

			// Blit to screen
			Graphics screenG(hdc);
			screenG.SetSmoothingMode(SmoothingModeHighQuality);
			screenG.DrawImage(&bmp, 0, 0, width, height);
		}
		else
		{
			// Fallback simple clear
			RECT fill = { 0,0,rc.right - rc.left, rc.bottom - rc.top };
			HBRUSH hBr = CreateSolidBrush(RGB(18, 18, 18));
			FillRect(hdc, &fill, hBr);
			DeleteObject(hBr);
		}

		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		s_hwnd = nullptr;
		EnsureGdiplusStop();
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void TestCanvas::Show(const char* title)
{
	if (s_hwnd) return;

	EnsureGdiplusStart();

	HINSTANCE hInst = GetModuleHandle(nullptr);
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = TestCanvas_WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"TestCanvasWindowClass";
	RegisterClassExW(&wc);

	std::wstring wtitle = Utf8ToWide(title ? title : "Gdiplus Test Canvas");

	// Create a simple overlapped resizable window
	s_hwnd = CreateWindowExW(0, wc.lpszClassName, wtitle.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
		nullptr, nullptr, hInst, nullptr);

	if (s_hwnd)
	{
		ShowWindow(s_hwnd, SW_SHOW);
		UpdateWindow(s_hwnd);
	}
	else
	{
		EnsureGdiplusStop();
	}
}

void TestCanvas::Close()
{
	if (!s_hwnd) return;
	DestroyWindow(s_hwnd);
	// s_hwnd cleared in WM_DESTROY
}

void TestCanvas::Invalidate()
{
	if (s_hwnd) InvalidateRect(s_hwnd, nullptr, FALSE);
}

HWND TestCanvas::GetHwnd()
{
	return s_hwnd;
}