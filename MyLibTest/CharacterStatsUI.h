#pragma once
#include "CharacterDetails.h"

class CharacterStatsUI
{
	public:
	// Show the character stats window. `title` is ANSI/UTF-8 C string.
	static void Show(const char* title);
	// Update the character stats display.
	static void UpdateStats(const CharacterDetails& details);
	// Close and destroy the window
	static void Close();
};

