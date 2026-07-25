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
#include <mmsystem.h>
#include "CombatLog.h"
#include <vector>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")   // for PlaySound

using namespace Gdiplus;

static HWND s_hwnd = nullptr;
static ULONG_PTR s_gdiplusToken = 0;
static int s_gdiplusRefCount = 0;

static const wchar_t* REG_KEY = L"Software\\sdgmaw\\MobRadar";
static const int DEFAULT_W = 430;
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
	float x;
	float y;
	float z;
	int Tier;
	int IsHidden;
	int IsTreasure;
	wchar_t name[64];
};

// Stored entity (UI-thread only)
struct MR_Entity {
	int id;
	float x;
	float y;
	float z;
	int Tier;
	int IsHidden;
	int IsTreasure;
	std::wstring name;
};

static float _maxRange = 100;
static float _partyDirX = 1;
static float _partyDirY = 0;
static float r2d = 180.0f / std::numbers::pi;
static float _mapCompletion = 0.5f;
static float _meleeRange = 328;
static float _proximityRange = 0.0f;  // 0 = disabled; draws its own ring when > _meleeRange
static int _lx = 0;
static int _ly = 0;

// UI storage (only accessed on UI thread)
static std::map<int, MR_Entity> s_entities;

// Boss warning sound state ---------------------------------------------------
static std::mutex s_soundMutex;
static std::mutex s_sound2Mutex;
static std::wstring s_bossWavPath;
static std::wstring s_WavPath;
static bool s_bossWarnEnabled = true;
static float s_bossWarnCooldown = 2.0f;
static DWORD s_lastBossWarnTick = 0;
static bool s_bossInRangePrev = false;

static bool s_playSound = true;
static float s_playSoundCooldown = 2.0f;
static DWORD s_lastPlaySoundTick = 0;

// Hidden monster warning sound state ----------------------------------------
static std::mutex s_hiddenSoundMutex;
static std::wstring s_hiddenWavPath;
static bool s_hiddenWarnEnabled = true;
static float s_hiddenWarnCooldown = 2.0f;
static DWORD s_lastHiddenWarnTick = 0;
static bool s_hiddenInRangePrev = false;

// Monster-appear tick sound state --------------------------------------------
// Plays once when monster count goes from 0 → >0 (rising edge only, no cooldown).
static std::mutex s_tickSoundMutex;
static std::wstring s_tickWavPath;      // path to tick .wav/.m4a
static bool s_tickSoundEnabled = true;
static int  s_prevMonsterCount = 0;    // monster count from previous paint call

// Treasure warning sound state -----------------------------------------------
// Plays once on the rising edge when any treasure (chest or ground item) comes into range.
static std::mutex s_treasureSoundMutex;
static std::wstring s_treasureWavPath;
static bool s_treasureWarnEnabled = true;
static bool s_treasureInRangePrev = false;
// Play the boss warning sound on a background thread so we never block the UI thread.
// Supports .wav (PlaySoundW) and .m4a / .mp3 / other MCI-capable formats (mciSendStringW).


static DWORD WINAPI PlaySoundThreadProc(LPVOID lp)
{
	wchar_t* path = reinterpret_cast<wchar_t*>(lp);
	if (path)
	{
		// Determine extension to pick the right playback API
		std::wstring wpath(path);
		bool useWav = false;
		auto dotPos = wpath.rfind(L'.');
		if (dotPos != std::wstring::npos)
		{
			std::wstring ext = wpath.substr(dotPos); // includes the dot
			// case-insensitive compare
			for (auto& c : ext) c = (wchar_t)towlower(c);
			useWav = (ext == L".wav");
		}

		if (useWav)
		{
//			CombatLog::AddText("\nPlaying boss warning sound (wav)...\n");
//			CombatLog::AddTextW(path);

			// Fast native path for .wav
			PlaySoundW(path, nullptr, SND_FILENAME | SND_NODEFAULT | SND_SYNC);
		}
		else
		{
//			CombatLog::AddText("\nPlaying boss warning sound (MCI)...\n");
//			CombatLog::AddTextW(path);

			// MCI path: supports .m4a, .mp3, .aac, .wma and other Windows-codec formats
			// Build a unique alias per call using thread ID to avoid conflicts
			wchar_t alias[32];
			swprintf_s(alias, L"bossalert%lu", GetCurrentThreadId());

			wchar_t openCmd[1024];
			swprintf_s(openCmd, L"open \"%s\" alias %s", path, alias);

			MCIERROR err = mciSendStringW(openCmd, nullptr, 0, nullptr);
			if (err == 0)
			{
				wchar_t playCmd[64];
				swprintf_s(playCmd, L"play %s wait", alias); // wait = synchronous playback
				mciSendStringW(playCmd, nullptr, 0, nullptr);

				wchar_t closeCmd[64];
				swprintf_s(closeCmd, L"close %s", alias);
				mciSendStringW(closeCmd, nullptr, 0, nullptr);
			}
			else
			{
				// MCI error
				wchar_t errMsg[128];
				swprintf_s(errMsg, L"\nMCI error %lu while opening sound file.\n", err);
				CombatLog::AddTextW(errMsg);
				s_bossWarnEnabled = false; // disable further attempts;
			}
		}

		delete[] path;
	}
	return 0;
}
static void PlaySoundFile(const std::wstring& wavPath)
{
	// Copy path for thread ownership
	wchar_t* pathCopy = new wchar_t[wavPath.size() + 1];
	wcscpy_s(pathCopy, wavPath.size() + 1, wavPath.c_str());
	HANDLE hThread = CreateThread(nullptr, 0, PlaySoundThreadProc, pathCopy, 0, nullptr);
	if (hThread) CloseHandle(hThread);
	else delete[] pathCopy;
}
// Check boss presence and trigger sound if needed (must be called on UI thread each paint)
static void CheckBossWarning(bool bossInRange, int bossTier)
{
	// Only trigger on the rising edge (boss just came into range) OR
	// boss is still in range but cooldown expired.
	if (!s_bossWarnEnabled) return;

	std::wstring wavPath;
	float cooldown;
	{
		std::lock_guard<std::mutex> lock(s_soundMutex);
		wavPath = s_bossWavPath;
		cooldown = s_bossWarnCooldown/(bossTier);
	}
	if (wavPath.empty()) return;

	if (bossInRange)
	{
		DWORD now = GetTickCount();
		DWORD cooldownMs = (DWORD)(cooldown * 1000.0f);
		bool cooldownExpired = (s_lastBossWarnTick == 0) ||
			((now - s_lastBossWarnTick) >= cooldownMs);

		// Play on rising edge OR when cooldown expires while boss remains in range
		bool risingEdge = bossInRange && !s_bossInRangePrev;
		if (risingEdge || (bossInRange && cooldownExpired))
		{
			s_lastBossWarnTick = now;
			//CombatLog::AddTextColorE("\nBoss in range! Playing warning sound...\n");
			// Copy path for thread ownership
			wchar_t* pathCopy = new wchar_t[wavPath.size() + 1];
			wcscpy_s(pathCopy, wavPath.size() + 1, wavPath.c_str());

			HANDLE hThread = CreateThread(nullptr, 0, PlaySoundThreadProc, pathCopy, 0, nullptr);
			if (hThread) CloseHandle(hThread);
			else delete[] pathCopy;
		}
	}
	else
	{
		// Reset cooldown timer when no boss is in range so next entry triggers immediately
		s_lastBossWarnTick = 0;
	}

	s_bossInRangePrev = bossInRange;
}


// Check hidden monster presence and trigger sound if needed (called on UI thread each paint).
static void CheckHiddenWarning(bool hiddenInRange)
{
	if (!s_hiddenWarnEnabled) return;

	std::wstring wavPath;
	float cooldown;
	{
		std::lock_guard<std::mutex> lock(s_hiddenSoundMutex);
		wavPath = s_hiddenWavPath;
		cooldown = s_hiddenWarnCooldown;
	}
	if (wavPath.empty()) return;

	if (hiddenInRange)
	{
		DWORD now = GetTickCount();
		DWORD cooldownMs = (DWORD)(cooldown * 1000.0f);
		bool cooldownExpired = (s_lastHiddenWarnTick == 0) ||
			((now - s_lastHiddenWarnTick) >= cooldownMs);

		bool risingEdge = hiddenInRange && !s_hiddenInRangePrev;
		if (risingEdge || (hiddenInRange && cooldownExpired))
		{
			s_lastHiddenWarnTick = now;
			PlaySoundFile(wavPath); // reuses existing helper
		}
	}
	else
	{
		// Reset so next detection triggers immediately
		s_lastHiddenWarnTick = 0;
	}

	s_hiddenInRangePrev = hiddenInRange;
}

// Check if monsters just appeared (0 -> >0) and play tick sound on rising edge.
// Must be called on UI thread each paint with the current visible monster count.
static void CheckMonsterAppearTick(int currentCount)
{
	if (!s_tickSoundEnabled) return;

	// Rising edge only: previous frame had 0 monsters, now we have some
	if (s_prevMonsterCount == 0 && currentCount > 0)
	{
		std::wstring wavPath;
		{
			std::lock_guard<std::mutex> lock(s_tickSoundMutex);
			wavPath = s_tickWavPath;
		}
		if (!wavPath.empty())
			PlaySoundFile(wavPath); // reuses existing shared helper
	}

	s_prevMonsterCount = currentCount;
}

// Check if treasure just came into range and play sound on the rising edge.
// Must be called on UI thread each paint with current treasure-in-range state.
static void CheckTreasureWarning(bool treasureInRange)
{
	if (!s_treasureWarnEnabled) return;

	// Rising edge only: treasure wasn't in range last frame, now it is
	if (treasureInRange && !s_treasureInRangePrev)
	{
		std::wstring wavPath;
		{
			std::lock_guard<std::mutex> lock(s_treasureSoundMutex);
			wavPath = s_treasureWavPath;
		}
		if (!wavPath.empty())
			PlaySoundFile(wavPath);
	}

	s_treasureInRangePrev = treasureInRange;
}
// Helper: create a small upward-pointing triangle path centered at (0,0) with specified size
static GraphicsPath* CreateTriangleUpPath(float size)
{
	GraphicsPath* path = new GraphicsPath();
	PointF points[3] = {
		PointF(0.0f, -size),
		PointF(-size, size),
		PointF(size, size)
	};
	path->AddPolygon(points, 3);
	return path;
}

// Helper: create a small downward-pointing triangle path centered at (0,0) with specified size
static GraphicsPath* CreateTriangleDownPath(float size)
{
	GraphicsPath* path = new GraphicsPath();
	PointF points[3] = {
		PointF(0.0f, size),
		PointF(size, -size),
		PointF(-size, -size)
	};
	path->AddPolygon(points, 3);
	return path;
}

// Helper: draw a 6-armed asterisk centered at (px, py) with the given arm length.
// Arms are drawn at 0°, 60°, and 120° (3 lines crossing through center).
static void DrawAsterisk(Graphics& g, Pen& pen, float px, float py, float size)
{
	const float angles[3] = { 0.0f, 60.0f, 120.0f };
	const float toRad = std::numbers::pi_v<float> / 180.0f;
	for (int a = 0; a < 3; ++a)
	{
		float rad = angles[a] * toRad;
		float cosA = std::cos(rad) * size;
		float sinA = std::sin(rad) * size;
		g.DrawLine(&pen, px - cosA, py - sinA, px + cosA, py + sinA);
	}
}
// Returns a Unicode 8-way compass arrow for the given angle in degrees
// (standard math orientation: 0 = East, 90 = North)
static const wchar_t* AngleToArrow(float deg)
{
	// Normalise to [0, 360)
	while (deg < 0.0f) deg += 360.0f;
	while (deg >= 360.0f) deg -= 360.0f;
	// 8 sectors of 45° each, starting North (337.5°..22.5° = N)
	// East = 0°, rotate so North is index 0
	int sector = (int)((deg + 22.5f) / 45.0f) % 8;
	// sectors: 0=E 1=NE 2=N 3=NW 4=W 5=SW 6=S 7=SE
	static const wchar_t* arrows[8] = { L"\u2192", L"\u2197", L"\u2191", L"\u2196",
										 L"\u2190", L"\u2199", L"\u2193", L"\u2198" };
	return arrows[sector];
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
	_lx = r.left; _ly = r.top;
	HKEY hk;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
	{
		DWORD left = (DWORD)r.left, top = (DWORD)r.top;
		RegSetValueExW(hk, L"Left", 0, REG_DWORD, (const BYTE*)&left, sizeof(left));
		RegSetValueExW(hk, L"Top", 0, REG_DWORD, (const BYTE*)&top, sizeof(top));
		RegCloseKey(hk);
	}
}

static bool LoadWindowPlacementFromRegistry(int& outLeft, int& outTop, int& outW, int& outH)
{
	outLeft = outTop = 0; outW = DEFAULT_W; outH = DEFAULT_H;
	HKEY hk;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_KEY, 0, KEY_READ, &hk) != ERROR_SUCCESS) return false;
	DWORD type = 0, data = 0, len = sizeof(DWORD);
	if (RegQueryValueExW(hk, L"Left", nullptr, &type, (LPBYTE)&data, &len) == ERROR_SUCCESS && type == REG_DWORD) outLeft = (int)data;
	if (RegQueryValueExW(hk, L"Top", nullptr, &type, (LPBYTE)&data, &len) == ERROR_SUCCESS && type == REG_DWORD) outTop = (int)data;
	RegCloseKey(hk);
	return (outW > 0 && outH > 0);
}

static wchar_t* Utf8ToWideAlloc(const char* s)
{
	if (!s) return nullptr;
	int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
	if (len <= 0) return nullptr;
	wchar_t* out = new wchar_t[len];
	MultiByteToWideChar(CP_UTF8, 0, s, -1, out, len);
	return out;
}

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

	DWORD ex = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
	DWORD style = WS_POPUP | WS_VISIBLE;

	int left, top, w, h;
	bool haveSaved = LoadWindowPlacementFromRegistry(left, top, w, h);
	if (!haveSaved) { w = DEFAULT_W; h = DEFAULT_H; left = CW_USEDEFAULT; top = CW_USEDEFAULT; }

	s_hwnd = CreateWindowExW(ex, wc.lpszClassName, wtitle.c_str(), style,
		left, top, w, h, nullptr, nullptr, hInst, nullptr);
	if (!s_hwnd) { EnsureGdiplusStop(); return; }

	SetLayeredWindowAttributes(s_hwnd, 0, 220, LWA_ALPHA);
	ShowWindow(s_hwnd, SW_SHOW);
	UpdateWindow(s_hwnd);
	_lx = left; _ly = top;

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
				int x = mi.rcWork.right - ww - 10;
				int y = mi.rcWork.top + 10;
				SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
				_lx = x; _ly = y;
			}
		}
	}
}

void MobRadar::Close() { if (s_hwnd) DestroyWindow(s_hwnd); }
void MobRadar::Invalidate() { if (s_hwnd) InvalidateRect(s_hwnd, nullptr, FALSE); }
HWND MobRadar::GetHwnd() { return s_hwnd; }
bool MobRadar::IsVisible() { return s_hwnd && IsWindowVisible(s_hwnd) != FALSE; }

void MobRadar::AddEntity(int id, float x, float y, float z, int tier, int isHidden, int isTreasure, const char* name)
{
	if (!s_hwnd) return;
	auto* pkt = new MR_EntityPost{ id, x, y, z, tier, isHidden, isTreasure };

	if (name && *name)
	{
		MultiByteToWideChar(CP_UTF8, 0, name, -1, pkt->name, 64);
		pkt->name[63] = L'\0';
	}
	else { pkt->name[0] = L'\0'; }
	PostMessageW(s_hwnd, WM_MR_ADD_ENTITY, 0, (LPARAM)pkt);

}

void MobRadar::RemoveEntity(int id) { if (s_hwnd) PostMessageW(s_hwnd, WM_MR_REMOVE_ENTITY, (WPARAM)id, 0); }
void MobRadar::ClearEntities() { if (s_hwnd) PostMessageW(s_hwnd, WM_MR_CLEAR, 0, 0); }

void MobRadar::SetPartyFacing(float x, float y)
{
	_partyDirX = x; _partyDirY = y;
	if (s_hwnd) PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);
}

void MobRadar::SetMaxRange(float range)
{
	_maxRange = range;
	if (s_hwnd) PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);
}
void MobRadar::SetProximityRange(float range)
{
	_proximityRange = range < 0.0f ? 0.0f : range;
	if (s_hwnd) PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);
}
void MobRadar::SetMapCompletion(float mu)
{
	_mapCompletion = mu;
	if (s_hwnd) PostMessageW(s_hwnd, WM_MR_UPDATE, 0, 0);
}

void MobRadar::SetVisible(bool visible)
{
	if (s_hwnd) PostMessageW(s_hwnd, WM_MR_VISIBILITY, (WPARAM)(visible ? 1 : 0), 0);
}

void MobRadar::SetBossWarnSound(const char* wavPath)
{
	std::lock_guard<std::mutex> lock(s_soundMutex);
	if (wavPath && *wavPath)
	{
		// Convert UTF-8 to wide and store
		int len = MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, nullptr, 0);
		s_bossWavPath.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, &s_bossWavPath[0], len);
		if (!s_bossWavPath.empty() && s_bossWavPath.back() == L'\0') s_bossWavPath.pop_back();
	}
	else
	{
		s_bossWavPath.clear();
	}
}

void MobRadar::SetHiddenWarnSound(const char* wavPath)
{
	std::lock_guard<std::mutex> lock(s_hiddenSoundMutex);
	if (wavPath && *wavPath)
	{
		int len = MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, nullptr, 0);
		s_hiddenWavPath.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, &s_hiddenWavPath[0], len);
		if (!s_hiddenWavPath.empty() && s_hiddenWavPath.back() == L'\0')
			s_hiddenWavPath.pop_back();
	}
	else
	{
		s_hiddenWavPath.clear();
	}
}

void MobRadar::SetHiddenWarnCooldown(float seconds)
{
	std::lock_guard<std::mutex> lock(s_hiddenSoundMutex);
	s_hiddenWarnCooldown = seconds < 0.5f ? 0.5f : seconds;
}

void MobRadar::SetHiddenWarnEnabled(bool enabled)
{
	s_hiddenWarnEnabled = enabled;
}
void MobRadar::SetPlaySound(const char* wavPath)
{
	if (s_playSound) return;//sound already playing, exit
	std::lock_guard<std::mutex> lock(s_soundMutex);
	if (wavPath && *wavPath)
	{
		// Convert UTF-8 to wide and store
		int len = MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, nullptr, 0);
		s_WavPath.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, &s_WavPath[0], len);
		if (!s_WavPath.empty() && s_WavPath.back() == L'\0') s_WavPath.pop_back();
		PlaySoundFile(s_WavPath);
	}
	else
	{
		s_WavPath.clear();
		s_playSound = false;
	}
}
void MobRadar::SetBossWarnCooldown(float seconds)
{
	std::lock_guard<std::mutex> lock(s_soundMutex);
	s_bossWarnCooldown = seconds < 0.5f ? 0.5f : seconds;
}

void MobRadar::SetBossWarnEnabled(bool enabled)
{
	s_bossWarnEnabled = enabled;
}
void MobRadar::SetMonsterTickSound(const char* wavPath)
{
	std::lock_guard<std::mutex> lock(s_tickSoundMutex);
	if (wavPath && *wavPath)
	{
		int len = MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, nullptr, 0);
		s_tickWavPath.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, &s_tickWavPath[0], len);
		if (!s_tickWavPath.empty() && s_tickWavPath.back() == L'\0')
			s_tickWavPath.pop_back();
	}
	else
	{
		s_tickWavPath.clear();
	}
}

void MobRadar::SetMonsterTickEnabled(bool enabled)
{
	s_tickSoundEnabled = enabled;
}

void MobRadar::SetTreasureWarnSound(const char* wavPath)
{
	std::lock_guard<std::mutex> lock(s_treasureSoundMutex);
	if (wavPath && *wavPath)
	{
		int len = MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, nullptr, 0);
		s_treasureWavPath.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, wavPath, -1, &s_treasureWavPath[0], len);
		if (!s_treasureWavPath.empty() && s_treasureWavPath.back() == L'\0')
			s_treasureWavPath.pop_back();
	}
	else
	{
		s_treasureWavPath.clear();
	}
}

void MobRadar::SetTreasureWarnEnabled(bool enabled)
{
	s_treasureWarnEnabled = enabled;
}

// Window procedure -----------------------------------------------------------
static LRESULT CALLBACK MobRadar_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		s_hwnd = hwnd;
		return 0;
	case WM_MOVE:
	case WM_SIZE:
		SaveWindowPlacementToRegistry(hwnd);
		return 0;
	case WM_NCHITTEST:
	{
		LRESULT hit = DefWindowProc(hwnd, WM_NCHITTEST, wParam, lParam);
		if (hit == HTCLIENT) {
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			ScreenToClient(hwnd, &pt);
			if (pt.y >= 0 && pt.y <= 28) return HTCAPTION;
		}
		return hit;
	}
	case WM_MR_UPDATE:
		InvalidateRect(hwnd, nullptr, FALSE);
		break;
	case WM_MR_ADD_ENTITY:
	{
		auto* pkt = reinterpret_cast<MR_EntityPost*>(lParam);
		if (pkt) {
			MR_Entity e{ pkt->id, pkt->x, pkt->y, pkt->z, pkt->Tier, pkt->IsHidden, pkt->IsTreasure, std::wstring(pkt->name) };
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
		return 1;
	case WM_MR_VISIBILITY:
	{
		if ((int)wParam) {
			SetWindowPos(hwnd, HWND_TOPMOST, _lx, _ly, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
			SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);
			UpdateWindow(hwnd);
			InvalidateRect(hwnd, nullptr, FALSE);
		}
		else {
			ShowWindow(hwnd, SW_HIDE);
		}
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
			Bitmap bmp(width, height, PixelFormat32bppPARGB);
			Graphics g(&bmp);
			const int PANEL_W = 130;

			g.SetSmoothingMode(SmoothingModeHighQuality);

			SolidBrush bg(Color(180, 20, 20, 20));
			g.FillRectangle(&bg, RectF(0, 0, (REAL)width, (REAL)height));

			float partyangle = (std::numbers::pi / 2.0f - _partyDirX) * r2d;
			Matrix mx, mxW;
			g.GetTransform(&mxW);
			//mx.Translate(width / 2.0f, height / 2.0f + 20);
			mx.Translate((width - PANEL_W) / 2.0f, height / 2.0f + 20);

			mx.Scale(1.0f, -1.0f);
			mx.Rotate(partyangle);
			g.SetTransform(&mx);

			REAL cx = 0, cy = 0;
			REAL radius = min(width - PANEL_W, height - 40) * 0.45f;
			Pen ring(Color(200, 100, 200, 100), 2.0f);
			g.DrawEllipse(&ring, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

			Pen ringInner(Color(200, 200, 100, 100));
			float muR = _meleeRange / _maxRange * radius;
			g.DrawEllipse(&ringInner, cx - muR, cy - muR, muR * 2.0f, muR * 2.0f);

			// Proximity ring — only shown when set above melee range
			if (_proximityRange > _meleeRange && _proximityRange < _maxRange)
			{
				float proxR = _proximityRange / _maxRange * radius;
				Pen ringProx(Color(180, 200, 128, 0), 1.5f);  // warm orange, dashed
				ringProx.SetDashStyle(DashStyleDash);
				g.DrawEllipse(&ringProx, cx - proxR, cy - proxR, proxR * 2.0f, proxR * 2.0f);
			}

			SolidBrush brushArc(Color(192, 128, 192, 192));
			g.FillPie(&brushArc, RectF(-radius, -radius, radius * 2.0f, radius * 2.0f), 45 - partyangle, 90);

			GraphicsPath* triUp = CreateTriangleUpPath(3.0f);
			GraphicsPath* triDown = CreateTriangleDownPath(3.0f);

			SolidBrush brushPlayer(Color(255, 64, 255, 64));
			g.FillEllipse(&brushPlayer, -5, -5, 10, 10);

			SolidBrush brushN(Color(255, 0, 0, 255));
			g.FillEllipse(&brushN, -3.0f, radius - 3.0f, 6.0f, 6.0f);

			FontFamily fontFamily(L"Segoe UI");
			Font font(&fontFamily, 11.0f, FontStyleRegular, UnitPixel);
			SolidBrush textBrush(Color(230, 230, 230));

			int count = 0, countBoss = 0, countProximity = 0;
			bool bossInRange = false;
			bool treasureInRange = false;
			int iBoss = 1;
			int iHidden = 0;

			// Chest blips: dark green asterisk  (IDs 10000-19999)
			Pen chestPen(Color(255, 0, 160, 60), 1.5f);
			Pen treasurePen(Color(255, 0, 100, 255), 1.5f);
			// At the top of WM_PAINT (before the entity loop), add the collection struct and vector:
			struct TreasureEntry {
				std::wstring name;
				float dist;
				float angleDeg;
				bool isChest;
				float dz;
			};
			std::vector<TreasureEntry> treasureList;

			for (auto& kv : s_entities)
			{
				const MR_Entity& ent = kv.second;

				float dr = sqrt(ent.x * ent.x + ent.y * ent.y);
				float mu = dr / _maxRange;
				float nx = 0, ny = 0;
				if (dr > 0) { nx = ent.x / dr; ny = ent.y / dr; }

				REAL px = (REAL)(nx * radius * mu);
				REAL py = (REAL)(ny * radius * mu);

				//// Treasure entities: draw as a blue asterisk and skip all mob logic
				//if (ent.IsTreasure)
				//{
				//	treasureInRange = true;
				//	bool isChest = (ent.id >= 10000 && ent.id < 20000);
				//	DrawAsterisk(g, isChest ? chestPen : treasurePen, px, py,isChest ? 7.5f : 5.0f);

				//	continue;
				//}
				// Inside the IsTreasure block (keep existing blip drawing, add list push):
				if (ent.IsTreasure)
				{
					treasureInRange = true;
					bool isChest = (ent.id >= 10000 && ent.id < 20000);
					DrawAsterisk(g, isChest ? chestPen : treasurePen, px, py, isChest ? 7.5f : 5.0f);

					float dist = sqrtf(ent.x * ent.x + ent.y * ent.y + ent.z * ent.z);
					// atan2 in standard math: 0=East, 90=North; convert to degrees
					float angleDeg = atan2f(ent.y, ent.x) * (180.0f / std::numbers::pi_v<float>);
					treasureList.push_back({ ent.name.empty() ? (isChest ? L"Chest" : L"Item") : ent.name,
											  dist, angleDeg, isChest, ent.z });
					continue;
				}

				if (ent.Tier > 1) bossInRange = true;

				REAL r = 4.0f * (float)ent.Tier;
				REAL rh = 2.0f;
				iBoss = max(iBoss, ent.Tier);
				if (ent.Tier > 1) countBoss++;

				SolidBrush brushEnt2(Color(255, 255, 100, 100));
				SolidBrush brushBoss(Color(255, 255, 0, 0));
				SolidBrush brushHidden(Color(128, 128, 128, 128));
				SolidBrush brushAbove(Color(255, 255, 32, 255));
				SolidBrush brushBelow(Color(255, 32, 255, 255));

				if (ent.IsHidden) 
				{
					iHidden++;
					g.FillEllipse(&brushHidden, px - rh, py - rh, rh * 2.0f, rh * 2.0f);
				}
				else if (ent.z > (_meleeRange / 2)) {
					Matrix mxSave; g.GetTransform(&mxSave);
					g.TranslateTransform(px, py);
					g.FillPath(&brushAbove, triUp);
					g.SetTransform(&mxSave);
				}
				else if (ent.z < -(_meleeRange / 2)) {
					Matrix mxSave; g.GetTransform(&mxSave);
					g.TranslateTransform(px, py);
					g.FillPath(&brushBelow, triDown);
					g.SetTransform(&mxSave);
				}
				else {
					if (ent.Tier > 1) g.FillEllipse(&brushBoss, px - r, py - r, r * 2.0f, r * 2.0f);
					else g.FillEllipse(&brushEnt2, px - r, py - r, r * 2.0f, r * 2.0f);
				}
				// count monsters (non-treasure, any visibility) within proximity radius
				if (!ent.IsTreasure && _proximityRange > _meleeRange)
				{
					if (dr <= _proximityRange)
						countProximity++;
				}
				count++;
			}

			delete triUp;
			delete triDown;

			// Check and trigger boss warning sound based on current boss presence
			CheckBossWarning(bossInRange,iBoss);
			bool bPlayHidden = false;
			if (iHidden > 0)
			{
				//only play hidden sound if ONLY hidden mobs are present
				bPlayHidden = (count <= iHidden);
			}
			
			CheckHiddenWarning(bPlayHidden);
			CheckMonsterAppearTick(count);
			CheckTreasureWarning(treasureInRange);

			g.SetTransform(&mxW);

			// Sort treasure list closest-first
			std::sort(treasureList.begin(), treasureList.end(),
				[](const TreasureEntry& a, const TreasureEntry& b) { return a.dist < b.dist; });

			// ── Right treasure panel ────────────────────────────────────────────────────
			Font panelFont(&fontFamily, 9.0f, FontStyleRegular, UnitPixel);
			Font panelFontBold(&fontFamily, 9.0f, FontStyleBold, UnitPixel);

			REAL panelX = (REAL)(width - PANEL_W);

			// Panel background
			SolidBrush panelBg(Color(210, 18, 18, 18));
			g.FillRectangle(&panelBg, RectF(panelX, 40.0f, (REAL)PANEL_W, (REAL)(height - 40)));

			// Separator line
			Pen sepPen(Color(200, 80, 80, 80), 1.0f);
			g.DrawLine(&sepPen, panelX, 40.0f, panelX, (REAL)height);

			// Header
			SolidBrush hdrBrush(Color(255, 160, 200, 160));
			g.DrawString(L"TREASURE", -1, &panelFontBold, PointF(panelX + 4.0f, 42.0f), &hdrBrush);

			// Entries
			SolidBrush chestDot(Color(255, 0, 160, 60));   // dark green
			SolidBrush itemDot(Color(255, 0, 150, 255));    // blue
			SolidBrush entryText(Color(230, 220, 220, 220));
			SolidBrush dimText(Color(180, 160, 160, 160));

			const REAL ROW_H = 14.0f;
			REAL ey = 54.0f;
			int maxRows = (int)((height - 54) / ROW_H);

			for (int ti = 0; ti < (int)treasureList.size() && ti < maxRows; ++ti)
			{
				const TreasureEntry& te = treasureList[ti];

				// Coloured dot
				SolidBrush& dot = te.isChest ? chestDot : itemDot;
				g.FillEllipse(&dot, panelX + 4.0f, ey + 2.0f, 6.0f, 6.0f);

				// Name label (truncated to ~5 chars to leave room for dist+arrow)
				std::wstring label = te.name;// .substr(0, 20);
				g.DrawString(label.c_str(), -1, &panelFont, PointF(panelX + 13.0f, ey), &entryText);

				// Distance (integer units)
				std::wostringstream ds;
				ds << (int)te.dz ;
				RectF distRect(panelX + 48.0f, ey, 46.0f, ROW_H);
				StringFormat sfR;
				sfR.SetAlignment(StringAlignmentFar);
				g.DrawString(ds.str().c_str(), -1, &panelFont, distRect, &sfR, &dimText);

				// Direction arrow
				const wchar_t* arrow = AngleToArrow(te.angleDeg);
				g.DrawString(arrow, -1, &panelFont, PointF(panelX + 118.0f, ey), &entryText);

				ey += ROW_H;
			}

			// Entry count if list is long
			if ((int)treasureList.size() > maxRows)
			{
				std::wostringstream more;
				more << L"+" << (treasureList.size() - maxRows) << L" more";
				g.DrawString(more.str().c_str(), -1, &panelFont, PointF(panelX + 4.0f, ey), &dimText);
			}
			// ── End right panel ─────────────────────────────────────────────────────────

			SolidBrush barBack(Color(255, 48, 48, 48));
			Pen borderPen(Color(255, 120, 120, 120), 1.0f);
			RectF slotRect(0, 0, (REAL)width, 40);
			g.FillRectangle(&barBack, slotRect);
			g.DrawRectangle(&borderPen, slotRect);
			RectF barBg(slotRect.X + 6, slotRect.Y + 6, width - 12.0f, slotRect.Height - 12.0f);
			RectF filled(barBg.X, barBg.Y, barBg.Width * _mapCompletion, barBg.Height);
			LinearGradientBrush lg(PointF(barBg.X, barBg.Y), PointF(barBg.GetRight(), barBg.Y), Color(255, 220, 20, 20), Color(255, 20, 220, 20));
			SolidBrush barOutline(Color(255, 80, 80, 80));
			g.FillRectangle(&barOutline, barBg);
			g.SetClip(filled); g.FillRectangle(&lg, barBg); g.ResetClip();

			std::wostringstream mp;
			mp << (_mapCompletion * 100) << L"%";
			g.DrawString(mp.str().c_str(), -1, &font, PointF(20, 2), &textBrush);

			// Range label — right-aligned in the top header bar
			Font smallFont(&fontFamily, 9.0f, FontStyleRegular, UnitPixel);
			Color cRange = Color(255, 200, 255, 200);
			if (_mapCompletion > 0.8f)
			{
				cRange = Color(255, 32, 64, 32); // dark if map is mostly complete)
			}
			SolidBrush rangeBrush(cRange); // soft green tint
			std::wostringstream rangeStr;
			rangeStr << L"Range: " << (int)_maxRange;
			RectF headerRect(0.0f, 2.0f, (REAL)width - 4.0f, 16.0f);
			StringFormat sfRight;
			sfRight.SetAlignment(StringAlignmentFar);
			sfRight.SetLineAlignment(StringAlignmentNear);
			g.DrawString(rangeStr.str().c_str(), -1, &smallFont, headerRect, &sfRight, &rangeBrush);


			std::wostringstream mobCount;
			mobCount << count << L" Mobs, " << countBoss << L" Bosses";
			if (_proximityRange > _meleeRange)
				mobCount << L"  [" << countProximity << L" near]";
			g.DrawString(mobCount.str().c_str(), -1, &font, PointF(0, 22), &textBrush);

			Graphics screenG(hdc);
			screenG.SetSmoothingMode(SmoothingModeHighQuality);
			screenG.DrawImage(&bmp, 0, 0, width, height);
		}
		EndPaint(hwnd, &ps);
		return 0;
	}
	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		SaveWindowPlacementToRegistry(hwnd);
		s_entities.clear();
		s_hwnd = nullptr;
		EnsureGdiplusStop();
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}