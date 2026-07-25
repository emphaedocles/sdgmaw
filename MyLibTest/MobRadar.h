#pragma once
#include <windows.h>
#include <string>

// Simple MobRadar window: frameless, topmost, translucent, double-buffered drawing.
// API is thread-safe via PostMessage for entity updates.
class MobRadar
{
public:
	// Show the radar. Restores last saved window position (if any).
	static void Show(const char* title = "Mob Radar");

	// Close the radar window.
	static void Close();

	// Invalidate/redraw (safe from other threads).
	static void Invalidate();

	// Add or update an entity position.
	static void AddEntity(int id, float x, float y, float z, int tier, int hidden, int isTreasure = 0, const char* name = nullptr);

	// Remove an entity by id.
	static void RemoveEntity(int id);

	// Clear all entities.
	static void ClearEntities();

	static void SetPartyFacing(float x, float y);

	// Get HWND (may be nullptr).
	static HWND GetHwnd();
	static void SetMaxRange(float range);
	static void SetProximityRange(float range);  // 0 disables; draws ring+count when > melee range
	static void SetMapCompletion(float mu);
	static void SetVisible(bool visible);
	static bool IsVisible();

	// Boss warning sound ---------------------------------------------------
	// Set the path to a .wav/.m4a file to play when a boss comes into range.
	// Pass nullptr or empty string to disable. Thread-safe.
	static void SetBossWarnSound(const char* wavPath);
	static void SetPlaySound(const char* wavPath);

	// Set the minimum seconds between successive boss warning sounds (default = 2).
	static void SetBossWarnCooldown(float seconds);

	// Enable or disable boss warning sound without clearing the file path.
	static void SetBossWarnEnabled(bool enabled);

	// Hidden monster warning sound -----------------------------------------
	// Set the path to a .wav/.m4a file to play when a hidden monster comes into range.
	// Pass nullptr or empty string to disable. Thread-safe.
	static void SetHiddenWarnSound(const char* wavPath);

	// Set the minimum seconds between successive hidden warning sounds (default = 5).
	static void SetHiddenWarnCooldown(float seconds);

	// Enable or disable hidden monster warning sound without clearing the file path.
	static void SetHiddenWarnEnabled(bool enabled);
	// Monster-appear tick sound --------------------------------------------
// Plays once when any monster appears in range after having 0 monsters (rising edge).
// Pass nullptr or empty string to disable. Thread-safe.
	static void SetMonsterTickSound(const char* wavPath);

	// Enable or disable the tick sound without clearing the file path.
	static void SetMonsterTickEnabled(bool enabled);

	// Treasure warning sound -----------------------------------------------
	// Set the path to a .wav/.m4a file to play when treasure comes into range.
	// Pass nullptr or empty string to disable. Thread-safe.
	static void SetTreasureWarnSound(const char* wavPath);

	// Enable or disable treasure warning sound without clearing the file path.
	static void SetTreasureWarnEnabled(bool enabled);
};