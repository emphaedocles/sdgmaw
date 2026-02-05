// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "myopen.h"
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
#include "mylibtest.h"
}


//add text appends to existing text
static int lua_AddText(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	myopen::AddTextColorE(text);
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
	myopen::AddTextColorE(s.c_str());
	//myopen::ScrollToEnd();

	return 0;

}
static int lua_ClearLog(lua_State* L)
{
	//SDGMawLogEx::ClearLog();
	myopen::ClearText();
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
	myopen::Dispose();
	return 0;
}
// Show a Windows message box from Lua
// Usage from Lua: mylibtest.showmsg("text", "title", flags)
// - flags is optional (MB_OK, MB_YESNO, MB_ICONINFORMATION, etc.)
static int lua_ShowMessage(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	const char* title = luaL_optstring(L, 2, "Message");
	myopen::OpenMe(text, title);
	myopen::AddText("\n");
	//myopen::ScrollToEnd();

	return 0;
}
static int lua_scrolltoend(lua_State* L)
{
	// Placeholder for future implementation
	// myopen::ScrollToEnd();
	myopen::ScrollToEnd();
	return 0;
}

// Register functions
static const luaL_Reg mylibtest_funcs[] = {
	{"addtext",lua_AddText},
	{"addline",lua_AddLine},
	{"clearlog",lua_ClearLog},
	{"disposelog",lua_DisposeLog},
	{"showmsg",lua_ShowMessage},
	{"scrolltoend",lua_scrolltoend}, // Placeholder for future implementation
	{NULL, NULL}
};
// Entry point for Lua 5.1
extern "C" __declspec(dllexport) int luaopen_mylibtest(lua_State* L) {
	luaL_register(L, "mylibtest", mylibtest_funcs); // Creates table and registers functions
	return 1; // Return the table
}
