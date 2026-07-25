#pragma once
#include <windows.h>
#include <string>

// Data for a single boss entry displayed in the BossStatsOverlay.
struct BossStatEntry
{
    int     id;          // unique entity id (matches radar entity id)
    std::string Name;    // display name (UTF-8)
    int     Health;      // current health
    int     MaxHealth;   // maximum health
    int     Tier;        // boss tier (>1 = boss)
};

// Frameless, topmost, translucent overlay that shows stats (name + health bar)
// for boss-tier monsters currently in radar range.
// Designed to sit just below the MobRadar window.
// Thread-safe: all mutations go via PostMessage.
class BossStatsOverlay
{
public:
    // Show the window. If MobRadar is open its saved position will be used to
    // auto-place below it; otherwise places at top-right corner.
    static void Show(const char* title = "Boss Stats");

    // Close and destroy the window.
    static void Close();

    // Add or update a boss entry. Pass Health <= 0 to trigger removal.
    static void UpdateBoss(int id, const char* name, int health, int maxHealth, int tier);

    // Remove a boss by id.
    static void RemoveBoss(int id);

    // Clear all boss entries.
    static void ClearBosses();

    // Show / hide without destroying.
    static void SetVisible(bool visible);
    static bool IsVisible();

    // Get HWND (may be nullptr if not shown yet).
    static HWND GetHwnd();
};