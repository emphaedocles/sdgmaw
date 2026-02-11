#pragma once
#include <windows.h>

// Simple MobRadar window: frameless, topmost, translucent, double-buffered drawing.
// API is thread-safe via PostMessage for entity updates.
class MobRadar
{
public:
	// Show the radar. Restores last saved window position (if any).
	// Default size = 512x512.
	static void Show(const char* title = "Mob Radar");

	// Close the radar window.
	static void Close();

	// Invalidate/redraw (safe from other threads).
	static void Invalidate();

	// Add or update an entity position. `id` is unique per entity. `x,y` are normalized [0..1].
	// `label` is optional (UTF-8). Safe to call from any thread.
	static void AddEntity(int id, float x, float y, int tier);

	// Remove an entity by id.
	static void RemoveEntity(int id);

	// Clear all entities.
	static void ClearEntities();

	static void SetPartyFacing(float x, float y);

	// Get HWND (may be nullptr).
	static HWND GetHwnd();
	static void SetMaxRange(float range);
	static void SetMapCompletion(float mu);
	
};