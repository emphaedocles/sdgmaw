
#pragma once
#include <windows.h>
#include <string>

class CharacterDpsUI
{
public:
	// Show the DPS window. `title` is ANSI/UTF-8 C string.
	static void Show(const char* title);

	// Update DPS for a named character (adds if new). `name` is ANSI/UTF-8.
	// `dps` is the latest DPS value to show.
	static void UpdateDps(const char* name, double dps);

	// Clear all entries and prepare for a new fight/session.
	static void Reset();

	// Close and destroy the window.
	static void Close();

	static void AddDpsEntry(std::string name, double dps, float timeStamp);

	static void NewGame() { Reset(); }

};
