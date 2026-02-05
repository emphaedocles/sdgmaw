#pragma once

#ifdef MYOPEN_EXPORTS
#define MYOPEN_API __declspec(dllexport)
#else
#define MYOPEN_API __declspec(dllimport)
#endif

namespace myopen {
	// Show the window (creates UI thread if needed). `text` and `title` are ANSI/UTF-8 C strings.
	MYOPEN_API void OpenMe(const char* text, const char* title);

	// Append text to the RichEdit (ANSI/UTF-8)
	MYOPEN_API void AddText(const char* text);

	// Append text with a specific color. 'color' is a COLORREF (use RGB(r,g,b)).
	MYOPEN_API void AddTextColor(const char* text, unsigned int color);

	// Append text with embedded color tokens (form-feed + 5 digits)
	MYOPEN_API void AddTextColorE(const char* text);

	// Clear all text from the RichEdit (thread-safe)
	MYOPEN_API void ClearText();

	// Scroll the RichEdit to the end (thread-safe)
	MYOPEN_API void ScrollToEnd();

	// Set maximum lines to keep (older lines removed from the start). min = 1.
	MYOPEN_API void SetMaxLines(int maxLines);

	// Close and destroy the window
	MYOPEN_API void Dispose();
}

