#include "pch.h"
#include "CharacterDpsUI.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include "DPSTrack.h"
#include "MyUtil.h"

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static constexpr int MAX_DPS_SLOTS = 5;
static HWND g_hWndDps = nullptr;
static ULONG_PTR g_gdiplusToken = 0;
static int g_gdiplusRef = 0;

struct DpsEntry {
	std::string Name;
	double Dps = 0.0;
	bool Empty() const { return Name.empty(); }
};

static DpsEntry g_entries[MAX_DPS_SLOTS];
static DPSTrack g_dpsTrack[MAX_DPS_SLOTS];

// Helpers -------------------------------------------------------------------
static void EnsureGdiplusStart()
{
	if (g_gdiplusRef++ == 0)
	{
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
	}
}

static void EnsureGdiplusStop()
{
	if (--g_gdiplusRef == 0)
	{
		if (g_gdiplusToken) { GdiplusShutdown(g_gdiplusToken); g_gdiplusToken = 0; }
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

// Window --------------------------------------------------------------------
static void Layout(HWND hwnd)
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	// nothing to move (we paint entire client)
	(void)rc;
}

static LRESULT CALLBACK DpsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
		InvalidateRect(hwnd, nullptr, TRUE);
		Layout(hwnd);
		return 0;
	case WM_ERASEBKGND:
		// we handle background in paint (double-buffer), avoid flicker
		return TRUE;
	case WM_DESTROY:
		g_hWndDps = nullptr;
		EnsureGdiplusStop();
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT rc;
		GetClientRect(hwnd, &rc);
		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;
		if (width <= 0 || height <= 0) { EndPaint(hwnd, &ps); return 0; }

		if (g_gdiplusToken)
		{
			// Offscreen bitmap
			Bitmap bmp(width, height, PixelFormat32bppPARGB);
			Graphics g(&bmp);
			g.SetSmoothingMode(SmoothingModeHighQuality);

			// Background
			g.Clear(Color(255, 24, 24, 24));

			// Layout metrics
			float margin = 10.0f;
			float slotH = (height - margin * 2) / (float)MAX_DPS_SLOTS;
			if (slotH < 24.0f) slotH = 24.0f;

			// Find max DPS to scale bars
			double maxDps = 0.0;
			for (int i = 0; i < MAX_DPS_SLOTS; ++i) if (!g_entries[i].Empty()) maxDps = max(maxDps, g_entries[i].Dps);
			if (maxDps <= 0.0) maxDps = 1.0; // avoid div by zero

			FontFamily ff(L"Segoe UI");
			Font labelFont(&ff, 11.0f, FontStyleRegular, UnitPixel);
			Font valueFont(&ff, 11.0f, FontStyleBold, UnitPixel);
			SolidBrush textBrush(Color(255, 230, 230, 230));
			SolidBrush barBack(Color(255, 48, 48, 48));
			Pen borderPen(Color(255, 120, 120, 120), 1.0f);

			for (int i = 0; i < MAX_DPS_SLOTS; ++i)
			{
				float top = margin + i * slotH;
				RectF slotRect(margin, top, width - margin * 2, slotH - 6.0f);

				// background for slot
				g.FillRectangle(&barBack, slotRect);
				g.DrawRectangle(&borderPen, slotRect);

				if (!g_entries[i].Empty())
				{
					// bar area (70% width for bar, remaining for name/value)
					float barAreaW = slotRect.Width * 0.60f;
					float textAreaX = slotRect.X + barAreaW + 8.0f;

					// filled fraction
					float fraction = (float)(g_entries[i].Dps / maxDps);
					if (fraction < 0.0f) fraction = 0.0f;
					if (fraction > 1.0f) fraction = 1.0f;

					RectF barBg(slotRect.X + 6.0f, slotRect.Y + 6.0f, barAreaW - 12.0f, slotRect.Height - 12.0f);
					// inner gradient
					RectF filled(barBg.X, barBg.Y, barBg.Width * fraction, barBg.Height);

					LinearGradientBrush lg(
						PointF(barBg.X, barBg.Y),
						PointF(barBg.GetRight(), barBg.Y),
						Color(255, 220, 20, 20),
						Color(255, 20, 220, 20));
					// soft border & background
					SolidBrush barOutline(Color(255, 80, 80, 80));
					g.FillRectangle(&barOutline, barBg);
					g.SetClip(filled);
					g.FillRectangle(&lg, barBg);
					g.ResetClip();

					// name
					std::wstring wname = Utf8ToWide(g_entries[i].Name.c_str());
					PointF labelPt(textAreaX, slotRect.Y + 6.0f);
					g.DrawString(wname.c_str(), -1, &labelFont, labelPt, &textBrush);

					// value (right aligned)
					std::string sv = ShortenNumber(g_entries[i].Dps, 3);
					std::wstring dpsVal = Utf8ToWide(sv.c_str());

					//dpsVal.append(std::to_wstring(g_entries[i].Dps));  //Utf8ToWide(ShortenNumber(g_entries[i].Dps, 3).c_str()).c_str() ;

					RectF valRect(textAreaX, slotRect.Y + 6.0f, slotRect.Width - (textAreaX - slotRect.X) - 8.0f, slotRect.Height - 12.0f);
					StringFormat sf;
					sf.SetAlignment(StringAlignmentFar);
					sf.SetLineAlignment(StringAlignmentCenter);
					g.DrawString(dpsVal.c_str(), -1, &valueFont, valRect, &sf, &textBrush);
				}
				else
				{
					// empty slot hint
					std::wstring hint = L"(empty)";
					PointF hintPt(slotRect.X + 8.0f, slotRect.Y + 6.0f);
					g.DrawString(hint.c_str(), -1, &labelFont, hintPt, &textBrush);
				}
			}

			// blit to screen
			Graphics screenG(hdc);
			screenG.DrawImage(&bmp, 0, 0, width, height);
		}
		else
		{
			// fallback simple GDI double-buffer
			HDC memDC = CreateCompatibleDC(hdc);
			if (memDC)
			{
				HBITMAP hbmp = CreateCompatibleBitmap(hdc, width, height);
				if (hbmp)
				{
					HBITMAP old = (HBITMAP)SelectObject(memDC, hbmp);
					HBRUSH bk = CreateSolidBrush(RGB(24, 24, 24));
					RECT rfill = { 0,0,width,height };
					FillRect(memDC, &rfill, bk);
					DeleteObject(bk);

					// Draw simple list
					int y = 8;
					for (int i = 0; i < MAX_DPS_SLOTS; ++i)
					{
						std::wstring line;
						if (!g_entries[i].Empty())
						{
							std::wostringstream ss;
							ss << Utf8ToWide(g_entries[i].Name.c_str()) << L" - " << g_entries[i].Dps << L" DPS";
							line = ss.str();
						}
						else
						{
							line = L"(empty)";
						}
						SetTextColor(memDC, RGB(230, 230, 230));
						SetBkMode(memDC, TRANSPARENT);
						RECT tr = { 8, y, width - 8, y + 18 };
						DrawTextW(memDC, line.c_str(), (int)line.size(), &tr, DT_LEFT | DT_SINGLELINE);
						y += 22;
					}

					BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

					SelectObject(memDC, old);
					DeleteObject(hbmp);
				}
				DeleteDC(memDC);
			}
		}

		EndPaint(hwnd, &ps);
	}
	return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// API -----------------------------------------------------------------------
void CharacterDpsUI::Show(const char* title)
{
	if (g_hWndDps) return;

	EnsureGdiplusStart();

	HINSTANCE hInst = GetModuleHandle(nullptr);
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = DpsWndProc;
	wc.hInstance = hInst;
	wc.lpszClassName = L"CharacterDpsWindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassExW(&wc);

	std::wstring wtitle = Utf8ToWide(title ? title : "DPS Meter");
	g_hWndDps = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
		wc.lpszClassName,
		wtitle.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 420, 260,
		nullptr, nullptr, hInst, nullptr);

	SetWindowPos(g_hWndDps, HWND_TOPMOST, 0, 50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	// initialize empty
	for (int i = 0; i < MAX_DPS_SLOTS; ++i) g_entries[i] = DpsEntry();
	for (int i = 0; i < MAX_DPS_SLOTS; ++i) g_dpsTrack[i] = DPSTrack("");

	// Show and then position to the right edge of the monitor work area (10px margin)
	ShowWindow(g_hWndDps, SW_SHOW);
	UpdateWindow(g_hWndDps);

	// Right-align on the monitor containing the window (respect taskbar/work area)
	{
		// compute window size (including non-client area)
		RECT wrect;
		if (GetWindowRect(g_hWndDps, &wrect))
		{
			int winW = wrect.right - wrect.left;
			int winH = wrect.bottom - wrect.top;

			HMONITOR hMon = MonitorFromWindow(g_hWndDps, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO mi;
			mi.cbSize = sizeof(mi);
			if (GetMonitorInfoW(hMon, &mi))
			{
				const int margin = 10;
				// use work area to avoid covering the taskbar
				int x = mi.rcWork.right - winW - margin;
				int y = mi.rcWork.top + margin;
				SetWindowPos(g_hWndDps, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
			}
		}
	}
}

void CharacterDpsUI::UpdateDps(const char* name, double dps)
{
	if (!name) return;
	// ensure UI exists
	if (!g_hWndDps) return;

	std::string sname(name);
	// find matching
	int slot = -1;
	for (int i = 0; i < MAX_DPS_SLOTS; ++i)
	{
		if (!g_entries[i].Empty() && g_entries[i].Name == sname) { slot = i; break; }
	}
	if (slot == -1)
	{
		// find first empty
		for (int i = 0; i < MAX_DPS_SLOTS; ++i) { if (g_entries[i].Empty()) { slot = i; break; } }
	}
	if (slot == -1)
	{
		// replace the one with lowest DPS
		double minD = DBL_MAX;
		int minIdx = 0;
		for (int i = 0; i < MAX_DPS_SLOTS; ++i) {
			if (g_entries[i].Dps < minD) { minD = g_entries[i].Dps; minIdx = i; }
		}
		slot = minIdx;
	}

	g_entries[slot].Name = sname;
	g_entries[slot].Dps = dps;

	InvalidateRect(g_hWndDps, nullptr, FALSE);
}

void CharacterDpsUI::Reset()
{
	for (int i = 0; i < MAX_DPS_SLOTS; ++i) g_entries[i] = DpsEntry();
	if (g_hWndDps) InvalidateRect(g_hWndDps, nullptr, TRUE);
}

void CharacterDpsUI::Close()
{
	if (!g_hWndDps) return;
	DestroyWindow(g_hWndDps);
	// g_hWndDps will be cleared in WM_DESTROY
}

void CharacterDpsUI::AddDpsEntry(std::string name, double dps, float timeStamp)
{
	int slot = -1;
	for (int i = 0; i < MAX_DPS_SLOTS; ++i)
	{
		if (!g_dpsTrack[i].Empty() && g_dpsTrack[i].Name == name) { slot = i; break; }
	}
	if (slot == -1)
	{
		// find first empty
		for (int i = 0; i < MAX_DPS_SLOTS; ++i) { if (g_dpsTrack[i].Empty()) { slot = i; break; } }
	}
	if (slot >= 0)
	{
		g_dpsTrack[slot].Name = name;

		g_dpsTrack[slot].AddEntry(timeStamp, dps);
		float avgDps = g_dpsTrack[slot].GetAverageDps(20.0f); // average over last x time units
		UpdateDps(name.c_str(), avgDps);

	}
}
