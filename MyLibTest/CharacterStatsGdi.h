#pragma once
//MyLibTest\CharacterStatsGdi.h
#pragma once
#include <windows.h>
#include "CharacterDetails.h"

// Lightweight GDI+ based control that draws a CharacterDetails using custom draw code.
// Create() makes a child window; SetDetails() updates the data and invalidates the control.
class CharacterStatsGdi
{
public:
	CharacterStatsGdi();
	~CharacterStatsGdi();

	// Create the child control. `id` becomes the child window ID (HMENU).
	// `rc` is the initial bounds in the parent's client coordinates.
	// Returns the HWND of the created child window (or nullptr on failure).
	HWND Create(HWND parent, int id, const RECT& rc);

	// Destroy the control (if created).
	void Destroy();

	// Update the details and request a repaint.
	void SetDetails(const CharacterDetails& details);

	// Utility: obtain the HWND (may be nullptr).
	HWND GetHwnd() const { return hwnd_; }

private:
	HWND hwnd_;
	CharacterDetails details_;

	// GDI+ initialization (single shared initialization for all instances).
	static void EnsureGdiplusStarted();
	static void EnsureGdiplusShutdown();
	static ULONG_PTR gdiplusToken_;
	static int gdiplusRefCount_;

	// Window procedure
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	// Paint routine
	void OnPaint();
}; 
