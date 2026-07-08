#pragma once
#include <windows.h>

// Simple test window with a blank GDI+ canvas.
// Use TestCanvas::Show() to create the window, TestCanvas::Invalidate() to request redraw,
// and TestCanvas::Close() to destroy it.
class TestCanvas
{
public:
    // Create and show the test window (no-op if already shown).
    static void Show(const char* title = "Gdiplus Test Canvas");

    // Destroy the window (no-op if not shown).
    static void Close();

    // Request a repaint (thread-safe).
    static void Invalidate();
	
    // Get the HWND (may be nullptr).
    static HWND GetHwnd();
};