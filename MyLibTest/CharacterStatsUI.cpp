#include "pch.h"
#include "CharacterStatsUI.h"
#include "CharacterDetails.h"
#include "CharacterStatsGdi.h"
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>

static constexpr int MAX_CHAR_SLOTS = 5;
static HWND g_hWnd = nullptr;
static CharacterStatsGdi* g_renderers[MAX_CHAR_SLOTS] = { nullptr };
static CharacterDetails g_details[MAX_CHAR_SLOTS];
static HINSTANCE g_hInstance = nullptr;

// Helpers -------------------------------------------------------------------
static std::wstring Utf8ToWide(const char* utf8)
{
	if (!utf8) return std::wstring();
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (len == 0) return std::wstring();
	std::wstring out; out.resize(len);
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], len);
	// remove trailing null inserted by resize with -1 source length
	if (!out.empty() && out.back() == L'\0') out.pop_back();
	return out;
}

static std::wstring FormatDetails(const CharacterDetails& d)
{
	std::wstringstream ss;
	ss << L"Name: " << Utf8ToWide(d.Name.c_str()) << L"\r\n";
	ss << L"Class: " << Utf8ToWide(d.Class.c_str()) << L"\r\n";
	ss << L"Level: " << d.Level << L"\r\n";
	ss << L"Health: " << d.Health << L" / " << d.MaxHealth << L"\r\n";
	ss << L"Mana: " << d.Mana << L" / " << d.MaxMana << L"\r\n";
	ss << L"Health Regen: " << d.HealthRegen << L"\r\n";
	ss << L"Mana Regen: " << d.ManaRegen << L"\r\n";
	return ss.str();
}

// Layout --------------------------------------------------------------------
static void LayoutRenderers(HWND parent)
{
	RECT rc;
	GetClientRect(parent, &rc);
	int width = rc.right - rc.left;
	int height = rc.bottom - rc.top;
	int slotHeight = height / MAX_CHAR_SLOTS;
	for (int i = 0; i < MAX_CHAR_SLOTS; ++i)
	{
		if (g_renderers[i] && g_renderers[i]->GetHwnd())
		{
			MoveWindow(g_renderers[i]->GetHwnd(), 0, i * slotHeight, width, slotHeight, TRUE);
		}
	}
}

// Window proc ---------------------------------------------------------------
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
		LayoutRenderers(hwnd);
		return 0;
	case WM_DESTROY:
		// destroy renderer instances and free resources
		for (int i = 0; i < MAX_CHAR_SLOTS; ++i)
		{
			if (g_renderers[i])
			{
				g_renderers[i]->Destroy();
				delete g_renderers[i];
				g_renderers[i] = nullptr;
			}
		}
		g_hWnd = nullptr;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// API -----------------------------------------------------------------------
void CharacterStatsUI::Show(const char* title)
{
	if (g_hWnd) return; // already shown

	g_hInstance = GetModuleHandle(nullptr);

	// Register window class
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = g_hInstance;
	wc.lpszClassName = L"CharacterStatsUIWindowClass";
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassExW(&wc);

	std::wstring wtitle = Utf8ToWide(title ? title : "Character Stats");

	g_hWnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
		wc.lpszClassName,
		wtitle.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 500,
		nullptr, nullptr, g_hInstance, nullptr);
		
	SetWindowPos(g_hWnd, HWND_TOPMOST, 10, 650, 0, 0,  SWP_NOSIZE | SWP_SHOWWINDOW);

	// Create renderer instances (GDI+ custom-draw controls)
	RECT clientRc;
	GetClientRect(g_hWnd, &clientRc);
	int totalHeight = clientRc.bottom - clientRc.top;
	int slotHeight = totalHeight / MAX_CHAR_SLOTS;
	for (int i = 0; i < MAX_CHAR_SLOTS; ++i)
	{
		RECT rc = { 0, i * slotHeight, 500, (i + 1) * slotHeight }; // initial width/height; will be re-laid out on WM_SIZE
		g_renderers[i] = new CharacterStatsGdi();
		g_renderers[i]->Create(g_hWnd, 2000 + i, rc);
		// initialize empty details
		g_details[i] = CharacterDetails();
		g_renderers[i]->SetDetails(g_details[i]);
	}

	ShowWindow(g_hWnd, SW_SHOW);
	UpdateWindow(g_hWnd);
}

void CharacterStatsUI::UpdateStats(const CharacterDetails& details)
{
	if (!g_hWnd) return;

	// Find matching slot by Name (if present); otherwise find first empty slot; if full, replace last.
	auto matchesName = [&](const std::string& a, const std::string& b) {
		return !a.empty() && !b.empty() && a == b;
		};

	int slot = -1;
	for (int i = 0; i < MAX_CHAR_SLOTS; ++i)
	{
		if (matchesName(g_details[i].Name, details.Name))
		{
			slot = i;
			break;
		}
	}
	if (slot == -1)
	{
		for (int i = 0; i < MAX_CHAR_SLOTS; ++i)
		{
			if (g_details[i].Name.empty())
			{
				slot = i;
				break;
			}
		}
	}
	if (slot == -1) slot = MAX_CHAR_SLOTS - 1;

	// store and show using GDI renderer
	g_details[slot] = details;
	if (g_renderers[slot])
	{
		g_renderers[slot]->SetDetails(details);
	}
}
void CharacterStatsUI::NewGame()
{
	// Clear stored details and update renderers so UI shows empty slots.
	for (int i = 0; i < MAX_CHAR_SLOTS; ++i)
	{
		g_details[i] = CharacterDetails(); // default-constructed, empty name => considered free
		if (g_renderers[i])
		{
			g_renderers[i]->SetDetails(g_details[i]);
		}
	}
}

void CharacterStatsUI::Close()
{
	if (!g_hWnd) return;
	DestroyWindow(g_hWnd);
	// g_hWnd will be cleared in WM_DESTROY
}