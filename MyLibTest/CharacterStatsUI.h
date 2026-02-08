#pragma once
#include "CharacterDetails.h"

class CharacterStatsUI
{
	public:
	// Show the character stats window. `title` is ANSI/UTF-8 C string.
	static void Show(const char* title);
	// Update the character stats display.
	static void UpdateStats(const CharacterDetails& details);
	// Prepare for a new game: clear existing character details and UI slots.
	static void NewGame();

	// Close and destroy the window
	static void Close();
};

