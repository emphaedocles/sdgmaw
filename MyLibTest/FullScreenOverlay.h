#pragma once
#include <windows.h>
#include <functional>

// Forward declare GDI+ Graphics type
namespace Gdiplus { class Graphics; }

// Full-screen, borderless, mouse-passthrough overlay window for GDI+ drawing.
// Use this for screen annotations, game overlays, etc.
class FullScreenOverlay
{
public:
    // Callback signature for custom drawing: void DrawCallback(Gdiplus::Graphics& g, int width, int height)
    using DrawCallback = std::function<void(Gdiplus::Graphics&, int, int)>;

    // Show the overlay (no-op if already shown). Restores to primary monitor full-screen.
    static void Show();

    // Close the overlay window.
    static void Close();

    // Set a custom draw callback that will be invoked during WM_PAINT.
    // The callback receives a GDI+ Graphics object and the window dimensions.
    // Thread-safe (uses PostMessage internally).
    static void SetDrawCallback(DrawCallback callback);

    // Request a redraw (thread-safe).
    static void Invalidate();

    // Set the global window alpha (0 = fully transparent, 255 = opaque). Default = 255.
    // This affects the entire window; individual drawing can use per-pixel alpha.
    static void SetAlpha(BYTE alpha);

    // Get HWND (may be nullptr).
    static HWND GetHwnd();

    // Query if the window is currently visible.
    static bool IsVisible();
};