// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "CombatLog.h"
#include "CharacterStatsUI.h"
#include "CharacterDetails.h"
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

	 CharacterStatsUI::UpdateStats(details);
	return 0;
}
static int lua_ShowCharStats(lua_State* L)
{
	const char* title = luaL_optstring(L, 1, "Character Stats");
	CharacterStatsUI::Show(title);
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
	{NULL, NULL}
};
// Entry point for Lua 5.1
extern "C" __declspec(dllexport) int luaopen_sdgmawix(lua_State* L) {
	luaL_register(L, "sdgmawix", sdgmawix_funcs); // Creates table and registers functions
	return 1; // Return the table
}
