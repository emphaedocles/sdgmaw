#pragma once

#ifdef CombatLog_EXPORTS
#define CombatLog_API __declspec(dllexport)
#else
#define CombatLog_API __declspec(dllimport)
#endif

namespace CombatLog {
	// Show the window (creates UI thread if needed). `text` and `title` are ANSI/UTF-8 C strings.
	CombatLog_API void OpenMe(const char* text, const char* title);

	// Show the window at a specified screen position (x,y in pixels).
	// If x or y is negative the position will be chosen by the system (default behavior).
	CombatLog_API void OpenMeAt(const char* text, const char* title, int x, int y);

	// Append text to the RichEdit (ANSI/UTF-8)
	CombatLog_API void AddText(const char* text);

	// Append text with a specific color. 'color' is a COLORREF (use RGB(r,g,b)).
	CombatLog_API void AddTextColor(const char* text, unsigned int color);

	// Append text with embedded color tokens (form-feed + 5 digits)
	CombatLog_API void AddTextColorE(const char* text);

	// Clear all text from the RichEdit (thread-safe)
	CombatLog_API void ClearText();

	// Scroll the RichEdit to the end (thread-safe)
	CombatLog_API void ScrollToEnd();

	// Set maximum lines to keep (older lines removed from the start). min = 1.
	CombatLog_API void SetMaxLines(int maxLines);

	// Set one of the 3 status panes (index 0..2) text (ANSI/UTF-8)
	CombatLog_API void SetStatus(int index, const char* text);

	// Close and destroy the window
	CombatLog_API void Dispose();
}

