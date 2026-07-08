#include "pch.h"
#include "MobRadar.h"
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <sstream>
#include <map>
#include <mutex>
#include <numbers>
#include "CombatLog.h"

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static HWND s_hwnd = nullptr;
static ULONG_PTR s_gdiplusToken = 0;
static int s_gdiplusRefCount = 0;

static const wchar_t* REG_KEY = L"Software\\sdgmaw\\MobRadar";
static const int DEFAULT_W = 300;
static const int DEFAULT_H = 320;

// Messages for thread-safe interaction
static const UINT WM_MR_ADD_ENTITY = WM_USER + 0x100;
static const UINT WM_MR_REMOVE_ENTITY = WM_USER + 0x101;
static const UINT WM_MR_CLEAR = WM_USER + 0x102;
static const UINT WM_MR_UPDATE = WM_USER + 0x103;
static const UINT WM_MR_VISIBILITY = WM_USER + 0x104;

// Entity structure passed via PostMessage (owner: receiver frees)
struct MR_EntityPost {
	int id;
	float x; // normalized 0..1
	float y; // normalized 0..1
	float z;
	int Tier;
	int IsHidden; // 0 or 1
};

// Stored entity (UI-thread only)
struct MR_Entity {
	int id;
	float x;
	float y;
	float z;
	int Tier;
	int IsHidden;
};
static float _maxRange = 100;
static float _partyDirX = 1;
static float _partyDirY = 0;
static float r2d = 180.0f / std::numbers::pi;
static float _mapCompletion = 0.5f;
static float _meleeRange = 328;
static int _lx;
static int _ly;

// UI storage (only accessed on UI thread)
static std::map<int, MR_Entity> s_entities;

// Helper: create a small upward-pointing triangle path centered at (0,0) with specified size
static GraphicsPath* CreateTriangleUpPath(float size)
{
	GraphicsPath* path = new GraphicsPath();
	// Triangle pointing up: top vertex at (0, -size), base vertices at (-size, size) and (size, size)
	PointF points[3];
	points[0] = PointF(0.0f, -size);           // top
	points[1] = PointF(-size, size);           // bottom-left
	points[2] = PointF(size, size);            // bottom-right
	path->AddPolygon(points, 3);
	return path;
}

// Helper: create a small downward-pointing triangle path centered at (0,0) with specified size
static GraphicsPath* CreateTriangleDownPath(float size)
{
	GraphicsPath* path = new GraphicsPath();
	// Triangle pointing down: bottom vertex at (0, size), base vertices at (-size, -size) and (size, -size)
	PointF points[3];
	points[0] = PointF(0.0f, size);            // bottom
	points[1] = PointF(size, -size);           // top-right
	points[2] = PointF(-size, -size);          // top-left
	path->AddPolygon(points, 3);
	return path;
}

// Helper: start/stop GDI+
static void EnsureGdiplusStart()
{
	if (s_gdiplusRefCount++ == 0)
	{
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&s_gdiplusToken, &gdiplusStartupInput, nullptr);
	}
}
static void EnsureGdiplusStop()
{
	if (--s_gdiplusRefCount == 0)
	{
		if (s_gdiplusToken) { GdiplusShutdown(s_gdiplusToken); s_gdiplusToken = 0; }
	}
}

// Helpers: registry save/restore position
static void SaveWindowPlacementToRegistry(HWND hwnd)
{
	if (!hwnd) return;
	RECT r; GetWindowRect(hwnd, &r);
	_lx = r.left;
	_ly = r.top;
	HKEY hk;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
	{
		DWORD left = (DWORD)r.left, top = (DWORD)r.top, width = (DWORD)(r.right - r.left), height = (DWORD)(r.bottom - r.top);
		RegSetValueExW(hk, L"Left", 0, REG_DWORD, (const BYTE*)&left, sizeof(left));
		RegSetValueExW(hk, L"Top", 0, REG_DWORD, (const BYTE*)&top, sizeof(top));
		RegCloseKey(hk);
	}
}
static bool LoadWindowPlacementFromRegistry(int& outLeft, int& outTop, int& outW, int& outH)
{
	outLeft = outTop = outW = outH = 0;
	outW = DEFAULT_W;
	outH = DEFAULT_H;
	HKEY hk;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hk) != ERROR_SUCCESS) return false;
	DWORD type = 0; DWORD data = 0; DWORD len = sizeof(DWORD);
	if (RegQueryValueExW(hk, L"Left", nullptr, &type, (LPBYTE)&data, &len) == ERROR_SUCCESS && type == REG_DWORD) outLeft = (int)data;
	if (RegQueryValueExW(hk, L"Top", nullptr, &type, (LPBYTE)&data, &len) == ERROR_SUCCESS && type == REG_DWORD) outTop = (int)data;
	RegCloseKey(hk);
	return (outW > 0 && outH > 0);
}

// Utility: convert UTF-8 C string to newly allocated wide string (caller must free with delete[])
static wchar_t* Utf8ToWideAlloc(const char* s)
{
	if (!s) return nullptr;
	int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
	if (len <= 0) return nullptr;
	wchar_t* out = new wchar_t[len];
	MultiByteToWideChar(CP_UTF8, 0, s, -1, out, len);
	return out;
}

// Window proc forward declaration
static LRESULT CALLBACK MobRadar_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// API --------------------------------------------------------------------
void MobRadar::Show(const char* title)
{
	if (s_hwnd) return;
	EnsureGdiplusStart();

	HINSTANCE hInst = GetModuleHandle(nullptr);
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = MobRadar_WndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"MobRadarWindowClass";
	RegisterClassExW(&wc);

	std::wstring wtitle;
	if (title) {
		int len = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
		wtitle.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, title, -1, &wtitle[0], len);
		if (!wtitle.empty() && wtitle.back() == L'\0') wtitle.pop_back();
	}
	else wtitle = L"Mob Radar";

	// Create frameless layered topmost window (WS_POPUP)
	DWORD ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
	DWORD style = WS_POPUP | WS_VISIBLE;

	// Load saved placement if present
	int left, top, w, h;
	bool haveSaved = LoadWindowPlacementFromRegistry(left, top, w, h);
	if (!haveSaved) { w = DEFAULT_W; h = DEFAULT_H; left = CW_USEDEFAULT; top = CW_USEDEFAULT; }

	s_hwnd = CreateWindowExW(ex, wc.lpszClassName, wtitle.c_str(), style,
		left, top, w, h, nullptr, nullptr, hInst, nullptr);
	if (!s_hwnd) { EnsureGdiplusStop(); return; }

	// make slightly translucent (global alpha)
	BYTE alpha = 220; // 0..255
	SetLayeredWindowAttributes(s_hwnd, 0, alpha, LWA_ALPHA);

	// show & update
	ShowWindow(s_hwnd, SW_SHOW);
	UpdateWindow(s_hwnd);
	_lx = left;
	_ly = top;
	// if we created with CW_USEDEFAULT, move to right-top of primary monitor with margin
	if (!haveSaved)
	{
		RECT wrect;
		if (GetWindowRect(s_hwnd, &wrect))
		{
			int ww = wrect.right - wrect.left;
			int wh = wrect.bottom - wrect.top;
			HMONITOR hMon = MonitorFromWindow(s_hwnd, MONITOR_DEFAULTTOPRIMARY);
			MONITORINFO mi; mi.cbSize = sizeof(mi);
			if (GetMonitorInfoW(hMon, &mi))
			{
				int margin = 10;
				int x = mi.rcWork.right - ww - margin;
				int y = mi.rcWork.top + margin;
				SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
				_lx = x;
				_ly = y;
			}
		}
	}
}

void MobRadar::Close()
{
	if (!s_hwnd) return;
	DestroyWindow(s_hwnd);
	// s_hwnd cleared on WM_DESTROY
}

void MobRadar::Invalidate()
{
	if (s_hwnd) InvalidateRect(s_hwnd, nullptr, FALSE);
}

void MobRadar::AddEntity(int id, float x, float y, float z, int tier, int isHidden)
{
	if (!s_hwnd) return;
	auto* pkt = new MR_EntityPost();
	pkt->id = id;
	pkt->x = x;
	pkt->y = y;
	pkt->z = z;
	pkt->Tier = tier;
	pkt->IsHidden = isHidden;

	PostMessageW(s_hwnd, WM_MR_ADD_ENTITY, 0, (LPARAM)pkt);
}

void MobRadar::RemoveEntity(int id)
{
	if (!s_hwnd) return;
	PostMessageW(s_hwnd, WM_MR_REMOVE_ENTITY, (WPARAM)id, 0);
}

void MobRadar::ClearEntities()
{
	if (!s_hwnd) return;
	PostMessageW(s_hwnd, WM_MR_CLEAR, 0, 0);
}

void MobRadar::SetPartyFacing(float x, float y)
{
	_partyDirX = x;
	_partyDirY = y;
	if (!s_hwnd) return;
	PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);
}

HWND MobRadar::GetHwnd()
{
	return s_hwnd;
}

void MobRadar::SetMaxRange(float range)
{
	_maxRange = range;
	if (!s_hwnd) return;
	PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);

}

void MobRadar::SetMapCompletion(float mu)
{
	_mapCompletion = mu;
	if (!s_hwnd) return;
	PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);

}


void MobRadar::SetVisible(bool visible)
{
	// Post to UI thread so visibility change happens on UI thread.
	if (!s_hwnd) return;
	PostMessageW(s_hwnd, WM_MR_VISIBILITY, (WPARAM)(visible ? 1 : 0), 0);
}

bool MobRadar::IsVisible()
{
	if (!s_hwnd) return false;
	return IsWindowVisible(s_hwnd) != FALSE;
}

// Window procedure ---------------------------------------------------------
static LRESULT CALLBACK MobRadar_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		s_hwnd = hwnd;
		// ensure transparent background for layered window
		return 0;

	case WM_MOVE:
	case WM_SIZE:
		// persist window placement on move/size (debounce not necessary for small apps)
		SaveWindowPlacementToRegistry(hwnd);
		return 0;

		// restrict drags to a top strip only
	case WM_NCHITTEST:
	{
		LRESULT hit = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
		if (hit == HTCLIENT)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ScreenToClient(hwnd, &pt);
			const int dragStripHeight = 28; // pixels from top that act as a caption
			if (pt.y >= 0 && pt.y <= dragStripHeight)
				return HTCAPTION;
		}
		return hit;
	}
	case WM_MR_UPDATE:
		InvalidateRect(hwnd, nullptr, FALSE);

		break;
	case WM_MR_ADD_ENTITY:
	{
		auto* pkt = reinterpret_cast<MR_EntityPost*>(lParam);
		if (pkt)
		{
			MR_Entity e;
			e.id = pkt->id;
			e.x = pkt->x;
			e.y = pkt->y;
			e.z = pkt->z;
			e.Tier = pkt->Tier;
			e.IsHidden = pkt->IsHidden;
			// update map (UI thread)
			s_entities[e.id] = e;
			delete pkt;
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		return 0;
	}

	case WM_MR_REMOVE_ENTITY:
	{
		int id = (int)wParam;
		auto it = s_entities.find(id);
		if (it != s_entities.end()) s_entities.erase(it);
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	}

	case WM_MR_CLEAR:
		s_entities.clear();
		InvalidateRect(hwnd, nullptr, FALSE);
		return 0;

	case WM_ERASEBKGND:
		// prevent flicker; we paint entire client in WM_PAINT
		return 1;
	case WM_MR_VISIBILITY:
	{
		int vis = (int)wParam;
		if (vis)
		{
			CombatLog::AddText("\nMobRadar: Show");
			ShowWindow(hwnd, SW_SHOWNA);
			// Use SetWindowPos with SWP_SHOWWINDOW and SWP_NOACTIVATE to reliably restore layered topmost window
			SetWindowPos(hwnd, HWND_TOPMOST, _lx, _ly, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
			// reapply layered alpha in case it was affected
			SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);
			UpdateWindow(hwnd);
			InvalidateRect(hwnd, nullptr, FALSE);

		}
		else
			CombatLog::AddText("\nMobRadar: Hide");
		ShowWindow(hwnd, SW_HIDE);
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
		if (width <= 0 || height <= 0) { EndPaint(hwnd, &ps); return 0; }

		if (s_gdiplusToken)
		{
			// Offscreen GDI+ bitmap
			Bitmap bmp(width, height, PixelFormat32bppPARGB);
			Graphics g(&bmp);
			g.SetSmoothingMode(SmoothingModeHighQuality);

			// translucent background (alpha composited)
			SolidBrush bg(Color(180, 20, 20, 20)); // semi-transparent dark
			g.FillRectangle(&bg, RectF(0, 0, (REAL)width, (REAL)height));

			float partyangle = (std::numbers::pi / 2.0f - _partyDirX) * r2d;

			Gdiplus::Matrix mx;
			Matrix mxW;
			g.GetTransform(&mxW);

			mx.Translate(width / 2.0f, height / 2.0f + 20);
			mx.Scale(1.0f, -1.0f);
			mx.Rotate(partyangle);
			g.SetTransform(&mx);


			// draw a simple range circle
			Pen ring(Color(200, 100, 200, 100), 2.0f);
			Pen ring2(Color(200, 100, 100, 200), 1.0f);
			REAL cx = 0;
			REAL cy = 0;
			REAL radius = min(width, height) * 0.45f;
			g.DrawEllipse(&ring, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

			float muRR = (_meleeRange * 5) / _maxRange * radius;
			g.DrawEllipse(&ring2, cx - muRR, cy - muRR, muRR * 2.0f, muRR * 2.0f);

			Pen ringInner(Color(200, 200, 100, 100));
			float muR = _meleeRange / _maxRange * radius;
			g.DrawEllipse(&ringInner, cx - muR, cy - muR, muR * 2.0f, muR * 2.0f);

			SolidBrush brushArc(Color(192, 128, 192, 192));
			g.FillPie(&brushArc, RectF(-radius, -radius, radius * 2.0f, radius * 2.0f), 45 - partyangle, 90);

			// Create triangle paths for elevation indicators (size 3 units)
			GraphicsPath* triUp = CreateTriangleUpPath(3.0f);
			GraphicsPath* triDown = CreateTriangleDownPath(3.0f);

			int count = 0;
			int countBoss = 0;
			int hidden = 0;
			int updown = 0;
			float dx0 = 0;
			float dy0 = 0;
			float dzRange = _meleeRange / 2;

			FontFamily fontFamily(L"Segoe UI");
			Font font(&fontFamily, 11.0f, FontStyleRegular, UnitPixel);
			SolidBrush textBrush(Color(230, 230, 230));

			SolidBrush brushEnt(Color(255, 64, 255, 64));
			g.FillEllipse(&brushEnt, -5, -5, 10, 10);

			SolidBrush brushN(Color(255, 0, 0, 255));
			REAL northX = 0;
			REAL northY = radius;
			REAL nr = 3;
			g.FillEllipse(&brushN, northX - nr, northY - nr, nr * 2.0f, nr * 2.0f);//draw blue dot for north indicator

			SolidBrush brushE(Color(255, 0, 128, 255));
			northX = radius;
			northY = 0;
			g.FillEllipse(&brushE, northX - nr, northY - nr, nr * 2.0f, nr * 2.0f);//draw blue dot for north indicator


			// draw entities as filled circles using normalized coords
			//going to do a double pass
			//first pass for hidden mobs and elevation indicators (triangles)
			//second pass for normal mobs and bosses (circles) so they render on top of hidden/elevation indicators
			SolidBrush brushEnt2(Color(255, 255, 128, 128));
			SolidBrush brushBoss(Color(255, 255, 0, 0));
			SolidBrush brushHidden(Color(128, 64, 64, 64));
			SolidBrush brushAbove(Color(255, 255, 255, 32));   // magenta for above
			SolidBrush brushBelow(Color(255, 255, 32, 255));   // cyan for below

			for (int pass = 0; pass < 3; pass++)
			{
				for (auto& kv : s_entities)
				{
					const MR_Entity& ent = kv.second;
					float dr = sqrt(ent.x * ent.x + ent.y * ent.y);
					float mu = dr / _maxRange;

					// clamp normalized coords
					float nx = 0;
					float ny = 0;
					if (dr > 0)
					{
						nx = (ent.x) / dr;
						ny = (ent.y) / dr;
					}
					REAL px = (REAL)(nx * radius * mu);
					REAL py = (REAL)(ny * radius * mu);
					REAL r = 4 * (float)ent.Tier;
					REAL rh = 2;

					if (pass == 0)
					{
						if (ent.IsHidden==1)
						{
							g.FillEllipse(&brushHidden, px - rh, py - rh, rh * 2.0f, rh * 2.0f);
						}
					}
					else if (pass == 1)
					{
						if (ent.IsHidden == 1)
						{
							// skip elevation indicators for hidden entities
							continue;
						}
						if (ent.z > (dzRange))
						{
							updown++;
							// Entity is above player - draw triangle pointing up
							Matrix mxSave;
							g.GetTransform(&mxSave);
							g.TranslateTransform(px, py);
							g.RotateTransform(-partyangle); // counter-rotate to keep triangle upright
							g.FillPath(&brushAbove, triDown);
							g.SetTransform(&mxSave);
						}
						else if (ent.z < -(dzRange))
						{
							updown++;
							// Entity is below player - draw triangle pointing down
							Matrix mxSave;
							g.GetTransform(&mxSave);
							g.TranslateTransform(px, py);
							g.RotateTransform(-partyangle); // counter-rotate to keep triangle upright
							g.FillPath(&brushBelow, triUp);
							g.SetTransform(&mxSave);
						}
					}
					else if (pass == 2)
					{
						if (ent.IsHidden>0 || ent.z<(-dzRange) || ent.z>(dzRange))
						{
							// skip drawing circle for entities with elevation indicators, to reduce clutter
							continue;
						}
						count++;

						if (ent.Tier > 1)
						{
							countBoss++;
						}

						// Entity at same level - draw circle
						if (ent.Tier > 1)
						{
							g.FillEllipse(&brushBoss, px - r, py - r, r * 2.0f, r * 2.0f);
						}
						else
						{
							g.FillEllipse(&brushEnt2, px - r, py - r, r * 2.0f, r * 2.0f);
						}
					}

				}
			}
			// Clean up triangle paths
			delete triUp;
			delete triDown;

			//reset world
			g.SetTransform(&mxW);
			//show map completion
							// background for slot
			SolidBrush barBack(Color(255, 48, 48, 48));
			Pen borderPen(Color(255, 120, 120, 120), 1.0f);
			RectF slotRect(0, 0, width, 40);
			g.FillRectangle(&barBack, slotRect);
			g.DrawRectangle(&borderPen, slotRect);
			RectF barBg(slotRect.X + 6.0f, slotRect.Y + 6.0f, width - 12.0f, slotRect.Height - 12.0f);
			// inner gradient
			RectF filled(barBg.X, barBg.Y, barBg.Width * _mapCompletion, barBg.Height);

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

			std::wostringstream mp;
			mp << (_mapCompletion * 100) << "%";
			g.DrawString(mp.str().c_str(), -1, &font, PointF(20, 2), &textBrush);

			std::wostringstream mobCount;
			mobCount << count << L" Total Mobs In Range, " << countBoss << " Bosses in Range," << updown << " above/below, " << hidden << " hidden";

			g.DrawString(mobCount.str().c_str(), -1, &font, PointF(0, 22), &textBrush);

			// Blit to screen using GDI+ DrawImage (layered window global alpha already set)
			Graphics screenG(hdc);
			screenG.SetSmoothingMode(SmoothingModeHighQuality);
			screenG.DrawImage(&bmp, 0, 0, width, height);
		}
		else
		{
			// fallback: simple GDI clear + dots
			HBRUSH b = CreateSolidBrush(RGB(20, 20, 20));
			RECT fill = { 0,0,width,height };
			FillRect(hdc, &fill, b);
			DeleteObject(b);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(230, 230, 230));
			for (auto& kv : s_entities)
			{
				const MR_Entity& ent = kv.second;
				int px = (int)(ent.x * width);
				int py = (int)(ent.y * height);
				Ellipse(hdc, px - 4, py - 4, px + 4, py + 4);
			}
		}

		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_CLOSE:
		DestroyWindow(hwnd); // triggers WM_DESTROY
		return 0;

	case WM_DESTROY:
		// persist placement on destroy
		SaveWindowPlacementToRegistry(hwnd);

		s_entities.clear();
		s_hwnd = nullptr;
		EnsureGdiplusStop();
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
