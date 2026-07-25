// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "CombatLog.h"
#include "CharacterStatsUI.h"
#include "CharacterDetails.h"
#include "CharacterDpsUI.h"
#include "MobRadar.h"
#include "BossStatsOverlay.h"

#include <string>

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "sdgmawix.h"
}


//add text appends to existing text
static int lua_AddText(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	CombatLog::AddTextColorE(text);
	//myopen::ScrollToEnd();

	return 0;

}
//add line appends a newline then the text
static int lua_AddLine(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);

	// Prepend a newline to the line
	std::string s = "\n";
	s += text ;
	CombatLog::AddTextColorE(s.c_str());
	//myopen::ScrollToEnd();

	return 0;

}
static int lua_ClearLog(lua_State* L)
{
	//SDGMawLogEx::ClearLog();
	CombatLog::ClearText();
	CombatLog::ShowCurrentFont();

	return 0;
}
static int lua_InitLog(lua_State* L)
{
	//SDGMawLogEx::InitLog();
	return 0;
}
static int lua_DisposeLog(lua_State* L)
{
	//SDGMawLogEx::DisposeLog();
	CombatLog::Dispose();
	return 0;
}
// Show a Windows message box from Lua
// Usage from Lua: mylibtest.showmsg("text", "title", flags)
// - flags is optional (MB_OK, MB_YESNO, MB_ICONINFORMATION, etc.)
static int lua_ShowMessage(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	const char* title = luaL_optstring(L, 2, "Message");
	CombatLog::OpenMe(text, title);
	CombatLog::AddText("\n");
	//myopen::ScrollToEnd();

	return 0;
}
static int lua_scrolltoend(lua_State* L)
{
	// Placeholder for future implementation
	// myopen::ScrollToEnd();
	CombatLog::ScrollToEnd();
	return 0;
}
static int lua_SetCharDetails(lua_State* L)
{
	CharacterDetails details = {};
	details.Name = luaL_optstring(L, 1, "Unknown");
	details.Class = luaL_optstring(L, 2, "Unknown");
	details.Level = (unsigned int)luaL_optinteger(L, 3, 1);
	details.Health = (int)luaL_optinteger(L, 4, 0);
	details.MaxHealth = (unsigned int)luaL_optinteger(L, 5, 0);
	details.Mana = (unsigned int)luaL_optinteger(L, 6, 0);
	details.MaxMana = (unsigned int)luaL_optinteger(L, 7, 0);
	details.ManaPoolMax = (unsigned int)luaL_optinteger(L, 8, 0);
	details.HealthRegen = (unsigned int)luaL_optinteger(L, 9, 0);
	details.ManaRegen = (unsigned int)luaL_optinteger(L, 10, 0);
	details.AC = (unsigned int)luaL_optinteger(L, 11, 0);
	details.StatusFx = luaL_optstring(L, 12, "");
	details.MeleeRating = luaL_optstring(L, 13, "");
	details.RangedRating = luaL_optstring(L, 14, "");
	details.SpellRating = luaL_optstring(L, 15, "");
	details.Vitality = luaL_optstring(L, 16, "");
	details.MapMeleeDamage = luaL_optstring(L, 17, "");
	details.MapRangedDamage = luaL_optstring(L, 18, "");
	details.MapHealing = luaL_optstring(L, 19, "");
	details.MapTotalDamage = luaL_optstring(L, 20, "");

	 CharacterStatsUI::UpdateStats(details);
	return 0;
}
static int lua_ShowCharStats(lua_State* L)
{
	const char* title = luaL_optstring(L, 1, "Character Stats");
	CharacterStatsUI::Show(title);
	return 0;
}
static int lua_NewGame(lua_State* L)
{
	CharacterStatsUI::NewGame();
	CharacterDpsUI::NewGame();

	return 0;
}
static int lua_showdps(lua_State* L)
{
	CharacterDpsUI::Show("SDG Maw DPS Meter");
	return 0;
}
static int lua_setstatustext(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	int index = (int)luaL_optinteger(L, 2, 0); // default to index 0 if not provided
	CombatLog::SetStatus(index, text); // Set status pane 0 (you can extend this to allow specifying index)
	return 0;
}
static int lua_adddpsentry(lua_State* L)
{
	std::string name = luaL_optstring(L, 1, "Unknown");
	float time = (float)luaL_optnumber(L, 2, 0);
	float damage = (float)luaL_optnumber(L, 3, 0);
	CharacterDpsUI::AddDpsEntry(name, damage, time);
}
static int lua_ShowRadar(lua_State* L)
{
	MobRadar::Show("SDG Maw Radar");
	BossStatsOverlay::Show("SDG Maw Boss Stats");
	return 0;
}
static int lua_CloseRadar(lua_State* L)
{
	MobRadar::Close();
	BossStatsOverlay::Close();
	return 0;
}
static int lua_ClearRadar(lua_State* L)
{
	MobRadar::ClearEntities();
	BossStatsOverlay::ClearBosses();
	return 0;
}
static int lua_AddRadarEntity(lua_State* L)
{
	int id = (int)luaL_optinteger(L, 1, 0);
	float x = (float)luaL_optnumber(L, 2, 0);
	float y = (float)luaL_optnumber(L, 3, 0);
	float z = (float)luaL_optnumber(L, 4, 0);
	int tier = (int)luaL_optinteger(L, 5,1);
	int hidden = (int)luaL_optinteger(L, 6, 0);
	int hp = (int)luaL_optinteger(L, 7, 0);
	int fullhp = (int)luaL_optinteger(L, 8, 0);
	const char* name = luaL_optstring(L, 9, "Unknown");
	int isTreasure = (int)luaL_optinteger(L, 10, 0);

	MobRadar::AddEntity(id, x, y, z, tier, hidden, isTreasure,name);
	if (tier > 1 && !isTreasure) // boss
	{
		BossStatsOverlay::UpdateBoss(id, name, hp, fullhp, tier);
	}
	return 0;
}

static int lua_SetProximityRange(lua_State* L)
{
	float r = (float)luaL_optnumber(L, 1, 0);
	MobRadar::SetProximityRange(r);
	return 0;
}
static int lua_SetRadarRange(lua_State* L)
{
	float r = (float)luaL_optnumber(L, 1, 100);
	MobRadar::SetMaxRange(r);
	return 0;
}
static int lua_RemoveRadarEntity(lua_State* L)
{
	int id = (int)luaL_optinteger(L, 1, 0);
	MobRadar::RemoveEntity(id);
	BossStatsOverlay::RemoveBoss(id);
	return 0;
}
static int lua_RadarPartyFacing(lua_State* L)
{
	float x = (float)luaL_optnumber(L, 1, 0);
	float y = (float)luaL_optnumber(L, 2, 0);
	MobRadar::SetPartyFacing(x, y);
	return 0;
}
static int lua_SetMapMu(lua_State* L)
{
	float mu = (float)luaL_optnumber(L, 1, 0);
	MobRadar::SetMapCompletion(mu);
	return 0;
}
static int lua_SetRadarVisible(lua_State* L)
{
	int vv= luaL_optinteger(L, 1, 1);

	bool v = false;
	if (vv == 1) v = true;
	MobRadar::SetVisible(v);
	return 0;
}
static int lua_CLTitle(lua_State* L)
{
	std::string name = luaL_optstring(L, 1, "");
	CombatLog::SetTextBox(name.c_str());
}
static int lua_SetBossWarnSound(lua_State* L)
{
	const char* wavPath = luaL_optstring(L, 1, "");
	MobRadar::SetBossWarnSound(wavPath);
	return 0;
}
static int lua_SetHiddenWarnSound(lua_State* L)
{
	const char* wavPath = luaL_optstring(L, 1, "");
	MobRadar::SetHiddenWarnSound(wavPath);
	return 0;
}
static int lua_SetMobAppearedSound(lua_State* L)
{
	const char* wavPath = luaL_optstring(L, 1, "");
	MobRadar::SetMonsterTickSound(wavPath);
	return 0;
}
static int lua_playsound(lua_State* L)
{
	const char* wavPath = luaL_optstring(L, 1, "");
	MobRadar::SetPlaySound(wavPath);
	return 0;
}
static int lua_SetTreasureWarnSound(lua_State* L)
{
	const char* wavPath = luaL_optstring(L, 1, "");
	MobRadar::SetTreasureWarnSound(wavPath);
	return 0;
}
static int lua_SetTreasureWarnEnabled(lua_State* L)
{
	bool enabled = (bool)luaL_optinteger(L, 1, 1);
	MobRadar::SetTreasureWarnEnabled(enabled);
	return 0;
}
static int lua_SetLogFile(lua_State* L)
{
	const char* path = luaL_optstring(L, 1, "");
	CombatLog::SetLogFile(path);
	return 0;
}
static int lua_CloseLogFile(lua_State* L)
{
	CombatLog::CloseLogFile();
	return 0;
}
// Register functions
static const luaL_Reg sdgmawix_funcs[] = {
	{"addtext",lua_AddText},
	{"addline",lua_AddLine},
	{"clearlog",lua_ClearLog},
	{"disposelog",lua_DisposeLog},
	{"showmsg",lua_ShowMessage},
	{"scrolltoend",lua_scrolltoend},
	{"setchardetails",lua_SetCharDetails},
	{"showcharstats",lua_ShowCharStats},
	{"newgame",lua_NewGame},
	{"setstatustext",lua_setstatustext},
	{"showdps",lua_showdps},
	{"adddpsentry",lua_adddpsentry},
	{"showradar",lua_ShowRadar},
	{"clearradar",lua_ClearRadar},
	{ "addradarentity",lua_AddRadarEntity},
	{"setradarrange",lua_SetRadarRange},
	{"removeradarentity",lua_RemoveRadarEntity},
	{"setpartydir",lua_RadarPartyFacing},
	{"setmapmu",lua_SetMapMu},
	{"setradarvisible",lua_SetRadarVisible},
	{"closeradar",lua_CloseRadar},
	{"clsettitle",lua_CLTitle},
	{"setbosswarnsound",lua_SetBossWarnSound},
	{"sethiddenwarnsound",lua_SetHiddenWarnSound},
	{"playsound",lua_playsound},
	{"setmobappearedsound",lua_SetMobAppearedSound},
	{"settreasurerwarnsound",lua_SetTreasureWarnSound},
	{"settreasurawnenabled",lua_SetTreasureWarnEnabled},
	 {"setproximityrange", lua_SetProximityRange},
		 {"setlogfile",   lua_SetLogFile},
	{"closelogfile", lua_CloseLogFile},
	{NULL, NULL}
};
// Entry point for Lua 5.1
extern "C" __declspec(dllexport) int luaopen_sdgmawix(lua_State* L) {
	luaL_register(L, "sdgmawix", sdgmawix_funcs); // Creates table and registers functions
	return 1; // Return the table
}
