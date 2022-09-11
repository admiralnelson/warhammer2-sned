//hacks to enable DLL again
/*
    Copyright (C) 2021  admiralnelson aka kris b.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    email: admiralofinternetmeme[at]outlook[dot]com 
    discord: z bb - tablet (bot easy)#1668
*/

// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <ctime>
#include <vector>
#include <mutex>
#include <deque>
#include <thread>
#include <iostream>
#include <condition_variable>
#include <future>
#include "MinHook.h"
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lobject.h"
}

#include "lrdb/server.hpp"


#define EXPORT extern "C" __declspec(dllexport)

/* environment variables that hold the search path for packages */
#define LUA_PATH	"LUA_PATH"
#define LUA_CPATH	"LUA_CPATH"

/* prefix for open functions in C libraries */
#define LUA_POF		"luaopen_"

/* separator for open functions in C libraries */
#define LUA_OFSEP	"_"


#define LIBPREFIX	"LOADLIB: "

#define POF		LUA_POF
#define LIB_FAIL	"open"


/* error codes for ll_loadfunc */
#define ERRLIB		1
#define ERRFUNC		2

const char* LicenseText = ""
"Warhammer 2 SNED (Script Native Extension DLL) Runtime Copyright(C) 2021 admiralnelson\n"
"This program comes with ABSOLUTELY NO WARRANTY "
"for details please read the supplied LICENSE.txt in the supplied archive\n"
"This is free software, and you are welcome to redistribute it\n"
"under certain conditions.\n";

//forward decl
static int StopDebugger(lua_State* L);


static void pusherror(lua_State* L) {
    int error = GetLastError();
    char buffer[128];
    if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, error, 0, buffer, sizeof(buffer), NULL))
        lua_pushstring(L, buffer);
    else
        lua_pushfstring(L, "system error %d\n", error);
}

static void** ll_register(lua_State* L, const char* path) {
    void** plib;
    lua_pushfstring(L, "%s%s", LIBPREFIX, path);
    lua_gettable(L, LUA_REGISTRYINDEX);  /* check library in registry? */
    if (!lua_isnil(L, -1))  /* is there an entry? */
        plib = (void**)lua_touserdata(L, -1);
    else {  /* no entry yet; create one */
        lua_pop(L, 1);
        plib = (void**)lua_newuserdata(L, sizeof(const void*));
        *plib = NULL;
        luaL_getmetatable(L, "_LOADLIB");
        lua_setmetatable(L, -2);
        lua_pushfstring(L, "%s%s", LIBPREFIX, path);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }
    return plib;
}

static lua_CFunction ll_sym(lua_State* L, void* lib, const char* sym) {
    lua_CFunction f = (lua_CFunction)GetProcAddress((HINSTANCE)lib, sym);
    if (f == NULL) pusherror(L);
    return f;
}

static void* ll_load(lua_State* L, const char* path) {
    HINSTANCE lib = LoadLibraryA(path);
    if (lib == NULL) pusherror(L);
    return lib;
}


static int ll_loadfunc(lua_State* L, const char* path, const char* sym) {
    void** reg = ll_register(L, path);
    if (*reg == NULL) *reg = ll_load(L, path);
    if (*reg == NULL)
        return ERRLIB;  /* unable to load library */
    else {
        lua_CFunction f = ll_sym(L, *reg, sym);
        if (f == NULL)
            return ERRFUNC;  /* unable to find function */
        lua_pushcfunction(L, f);
        return 0;  /* return function */
    }
}

static const char* pushnexttemplate(lua_State* L, const char* path) {
    const char* l;
    while (*path == *LUA_PATHSEP) path++;  /* skip separators */
    if (*path == '\0') return NULL;  /* no more templates */
    l = strchr(path, *LUA_PATHSEP);  /* find next separator */
    if (l == NULL) l = path + strlen(path);
    lua_pushlstring(L, path, l - path);  /* template */
    return l;
}

static int readable(const char* filename) {
    FILE* f = fopen(filename, "r");  /* try to open file */
    if (f == NULL) return 0;  /* open failed */
    fclose(f);
    return 1;
}

static const char* findfile(lua_State* L, const char* name,
    const char* pname) {
    const char* path;
    name = luaL_gsub(L, name, ".", LUA_DIRSEP);
    lua_getfield(L, LUA_ENVIRONINDEX, pname);
    path = lua_tostring(L, -1);
    if (path == NULL)
        luaL_error(L, LUA_QL("package.%s") " must be a string", pname);
    lua_pushstring(L, "");  /* error accumulator */
    while ((path = pushnexttemplate(L, path)) != NULL) {
        const char* filename;
        filename = luaL_gsub(L, lua_tostring(L, -1), LUA_PATH_MARK, name);
        if (readable(filename))  /* does file exist and is readable? */
            return filename;  /* return that file name */
        lua_pop(L, 2);  /* remove path template and file name */
        luaO_pushfstring(L, "\n\tno file " LUA_QS, filename);
        lua_concat(L, 2);
    }
    return NULL;  /* not found */
}

static const char* mkfuncname(lua_State* L, const char* modname) {
    const char* funcname;
    const char* mark = strchr(modname, *LUA_IGMARK);
    if (mark) modname = mark + 1;
    funcname = luaL_gsub(L, modname, ".", LUA_OFSEP);
    funcname = lua_pushfstring(L, POF"%s", funcname);
    lua_remove(L, -2);  /* remove 'gsub' result */
    return funcname;
}

static void loaderror(lua_State* L, const char* filename) {
    luaL_error(L, "error loading module " LUA_QS " from file " LUA_QS ":\n\t%s",
        lua_tostring(L, 1), filename, lua_tostring(L, -1));
}

static int loader_C(lua_State* L) {
    const char* funcname;
    const char* name = luaL_checkstring(L, 1);
    const char* filename = findfile(L, name, "cpath");
    if (filename == NULL) return 1;  /* library not found in this path */
    funcname = mkfuncname(L, name);
    if (ll_loadfunc(L, filename, funcname) != 0)
        loaderror(L, filename);
    return 1;  /* library loaded successfully */
}

static int ll_loadlib(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const char* init = luaL_checkstring(L, 2);
    int stat = ll_loadfunc(L, path, init);
    if (stat == 0)  /* no errors? */
        return 1;  /* return the loaded function */
    else {  /* error; error message is on stack top */
        lua_pushnil(L);
        lua_insert(L, -2);
        lua_pushstring(L, (stat == ERRLIB) ? LIB_FAIL : "init");
        return 3;  /* return nil, error message, and where */
    }
}

static void ll_unloadlib(void* lib) {
    FreeLibrary((HINSTANCE)lib);
}

static int gctm(lua_State* L) {
    void** lib = (void**)luaL_checkudata(L, 1, "_LOADLIB");
    if (*lib) ll_unloadlib(*lib);
    *lib = NULL;  /* mark library as closed */

    //close the debugger too if it's still on
    StopDebugger(L);

    return 0;
}

//end

bool GetColour(short& ret) 
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return false;
    ret = info.wAttributes;
    return true;
}

static int print2(lua_State* L)
{
    std::string s = std::string(luaL_checkstring(L, 1));
    std::cout << s << std::endl;

    return 0;
}

static int PrintError(lua_State* L)
{
    std::string s = std::string(luaL_checkstring(L, 1));
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    short CurrentColour;
    GetColour(CurrentColour);

    SetConsoleTextAttribute(hConsole, BACKGROUND_RED | FOREGROUND_INTENSITY);

    std::cout << s;

    SetConsoleTextAttribute(hConsole, CurrentColour);

    return 0;
}

static int PrintWarning(lua_State* L)
{
    std::string s = std::string(luaL_checkstring(L, 1));
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    short CurrentColour;
    GetColour(CurrentColour);
    SetConsoleTextAttribute(hConsole, BACKGROUND_GREEN | BACKGROUND_RED );

    std::cout << s;
    SetConsoleTextAttribute(hConsole, CurrentColour);

    return 0;
}

static lrdb::server *ServerInstance = nullptr;
static int StartDebugger(lua_State* L)
{
    if (ServerInstance == nullptr)
    {
        std::cout << "=======Debugger started, game will freeze!=======" << std::endl;
        ServerInstance = new lrdb::server(21110);
        ServerInstance->reset(L);
    }
    return 0;
}

static int StopDebugger(lua_State* L)
{
    if (ServerInstance != nullptr)
    {
        ServerInstance->exit();
        delete ServerInstance;
        ServerInstance = nullptr;
        std::cout << "=======Debugger stopped=======" << std::endl;
    }
    return 0;
}

static int ExecuteLuaScriptDebug(lua_State* L)
{
    StartDebugger(L);
    std::string luaScriptPath = luaL_checkstring(L, 1);
    bool result = luaL_dofile(L, luaScriptPath.c_str());
    lua_pushboolean(L, result);
    return 1;
}


static int LuaPrint(lua_State* L)
{
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; i++)
    {
        if (lua_isstring(L, i))
        {
            /* Pop the next arg using lua_tostring(L, i) and do your print */
            std::cout << lua_tostring(L, i);
        }
        else
        {
            if (lua_isnumber(L, i))
            {
                std::cout << lua_tonumber(L, i);
            }
            if (lua_isboolean(L, i))
            {
                std::cout << lua_toboolean(L, i) ? "true" : "false";
            }
            if (lua_isfunction(L, i))
            {
                std::cout << "<lua function located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            if (lua_isuserdata(L, i))
            {
                std::cout << "<lua userdata located at: " << std::hex << lua_touserdata(L, i) << ">";
            }
            if (lua_istable(L, i))
            {
                std::cout << "<lua table located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            if (lua_iscfunction(L, i))
            {
                std::cout << "<lua native function located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            if (lua_islightuserdata(L, i))
            {
                std::cout << "<lua lightuserdata located at: " << lua_topointer(L, i) << ">";
            }
            if (lua_isthread(L, i))
            {
                std::cout << "<lua thread located at: " << lua_topointer(L, i) << ">";
            }
            if (lua_isnoneornil(L, i))
            {
                std::cout << "nil";
            }
        }
        std::cout << "  ";
    }
    std::cout << std::endl;
    return 0;
}

typedef int(__fastcall* LUAOPEN_PACKAGE)(lua_State* L);
typedef int(__fastcall* LUAL_ERROR)(lua_State* L, const char* fmt, ...);
static LUAOPEN_PACKAGE g_fp_luaopen_package = nullptr;
static LUAL_ERROR g_lual_error = nullptr;
uint64_t __fastcall hf_luaopen_package(lua_State* L)
{
    //MessageBoxA(nullptr, "Lua has been patched sire.", "*Finger crossed*", MB_OK);
    /* create new type _LOADLIB */
    luaL_newmetatable(L, "_LOADLIB");
    lua_pushcfunction(L, gctm);
    lua_setfield(L, -2, "__gc");
    lua_register(L, "require2", ll_loadlib);
    lua_register(L, "print2", print2);
    lua_register(L, "PrintError", PrintError);
    lua_register(L, "PrintWarning", PrintWarning);
    lua_register(L, "StartDebugger", StartDebugger);
    lua_register(L, "StopDebugger", StopDebugger);
    lua_register(L, "ExecuteLuaScriptDebug", ExecuteLuaScriptDebug);
    lua_register(L, "print", LuaPrint);
    HWND currentWindow = GetActiveWindow();
    SetWindowText(currentWindow, L"Total Warhammer 2 Injected with SNED (Script Native Enchancer DLL)");

    return g_fp_luaopen_package(L);
}

int hf_luaL_error(lua_State* L, const char* fmt, ...)
{
    int out = NULL;
    va_list args, argsForMe;
    va_start(args, fmt);
    va_copy(argsForMe, args);
    vprintf(fmt, argsForMe);
    if (g_lual_error)
    {
        out = g_lual_error(L, fmt, args);
    }
    va_end(argsForMe);
    va_end(args);
    return out;
}

uintptr_t FindDMAAddy(uintptr_t ptr, std::vector<unsigned int> offsets)
{
    uintptr_t addr = ptr;
    for (unsigned int i = 0; i < offsets.size(); ++i)
    {
        addr = *(uintptr_t*)addr;
        addr += offsets[i];
    }
    return addr;
}


EXPORT BOOL Test()
{
    #pragma EXPORT
    printf("hellow");
    return true;
}

bool RedirectConsoleIO()
{
    bool result = true;


    FILE* fp;

    // Redirect STDIN if the console has an input handle
    if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONIN$", "r", stdin) != 0)
            result = false;
        else
            setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT if the console has an output handle
    if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0)
            result = false;
        else
            setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR if the console has an error handle
    if (GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0)
            result = false;
        else
            setvbuf(stderr, NULL, _IONBF, 0);

    // Make C++ standard streams point to console as well.
    std::ios::sync_with_stdio(true);

    return result;
}

bool ReleaseConsole()
{
    bool result = true;
    FILE* fp;

    // Just to be safe, redirect standard IO to NUL before releasing.

    // Redirect STDIN to NUL
    if (freopen_s(&fp, "NUL:", "r", stdin) != 0)
        result = false;
    else
        setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT to NUL
    if (freopen_s(&fp, "NUL:", "w", stdout) != 0)
        result = false;
    else
        setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR to NUL
    if (freopen_s(&fp, "NUL:", "w", stderr) != 0)
        result = false;
    else
        setvbuf(stderr, NULL, _IONBF, 0);

    // Detach from console
    if (!FreeConsole())
        result = false;

    return result;
}

bool CreateNewConsole()
{
    bool result = false;

    // Attempt to create new console
    if (AllocConsole())
    {
        result = RedirectConsoleIO();
    }

    return result;
}

bool AttachParentConsole()
{
    bool result = false;

    // Release any current console and redirect IO to NUL
    ReleaseConsole();

    // Attempt to attach to parent process's console
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        result = RedirectConsoleIO();
    }

    return result;
}

typedef __int64(__fastcall* PRINT_ENGINE)(__int64 arg1, const char* format, ...);
static PRINT_ENGINE g_print_engine_something_original = nullptr;

__int64 EngineLogger(__int64 arg1, const char* format, ...)
{
    __int64 out = NULL;
    va_list args, argsForMe;
    va_start(args, format);
    va_copy(argsForMe, args);
    vprintf(format, argsForMe);
    if (g_print_engine_something_original)
    {
        out =  g_print_engine_something_original(arg1, format, args);
    }
    va_end(argsForMe);
    va_end(args);
    return out;
}

static PRINT_ENGINE g_print_engine_something_2_original = nullptr;
__int64 EngineLogger2(__int64 arg1, const char* format, ...)
{
    __int64 out = NULL;
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    if (g_print_engine_something_2_original)
    {
        out = g_print_engine_something_2_original(arg1, format, args);
    }
    va_end(args);
    return out;
}


typedef const char* (__fastcall* ERROR_MSG)(int code);
static ERROR_MSG g_error_reason_msg = nullptr;

const char* __fastcall ErrorReasonMsg(int code)
{
    const char* result;

    switch (code)
    {
    case 0:
        result = "This feature is not implemented";
        break;
    case 1:
        result = "The operation was successful";
        break;
    case 2:
        result = "The operation failed";
        break;
    case 3:
        result = "The operation succeeded partially";
        break;
    case 4:
        result = "Incompatible formats";
        break;
    case 5:
        result = "The stream is already connected to another node";
        break;
    case 6:
        result = "Trying to open a file when its name was not set";
        break;
    case 7:
        result = "An unexpected value causes the file to be invalid";
        break;
    case 8:
        result = "The file header is too large";
        break;
    case 9:
        result = "The maximum was reached";
        break;
    case 10:
        result = "Inputs are currently used";
        break;
    case 11:
        result = "Outputs are currently used";
        break;
    case 12:
        result = "The name is invalid";
        break;
    case 13:
        result = "The name is already in use";
        break;
    case 14:
        result = "The ID is invalid";
        break;
    case 15:
        result = "The ID was not found";
        break;
    case 16:
        result = "The InstanceID is invalid";
        break;
    case 17:
        result = "No more data is available from the source";
        break;
    case 18:
        result = "There is no child (source) associated with the node";
        break;
    case 19:
        result = "The StateGroup already exists";
        break;
    case 20:
        result = "The StateGroup is not a valid channel";
        break;
    case 21:
        result = "The child already has a parent";
        break;
    case 22:
        result = "The language is invalid (applies to the Low-Level I/O)";
        break;
    case 23:
        result = "It is not possible to add itself as its own child";
        break;
    case 29:
        result = "This user is not there";
        break;
    case 30:
        result = "Not in use";
        break;
    case 31:
        result = "Something is not within bounds";
        break;
    case 32:
        result = "Something was not within bounds and was relocated to the nearest OK value";
        break;
    case 33:
        result = "The sound has 3D parameters";
        break;
    case 34:
        result = "The sound does not have 3D parameters";
        break;
    case 35:
        result = "The item could not be added because it was already in the list";
        break;
    case 36:
        result = "This path is not known";
        break;
    case 37:
        result = "Stuff in vertices before trying to start it";
        break;
    case 38:
        result = "Only a running path can be paused";
        break;
    case 39:
        result = "Only a paused path can be resumed";
        break;
    case 40:
        result = "This path is already there";
        break;
    case 41:
        result = "This path is not there";
        break;
    case 42:
        result = "Unknown in our voices list";
        break;
    case 43:
        result = "The consumer needs more";
        break;
    case 44:
        result = "The consumer does not need more";
        break;
    case 45:
        result = "The provider has available data";
        break;
    case 46:
        result = "The provider does not have available data";
        break;
    case 47:
        result = "Not enough space to load bank";
        break;
    case 48:
        result = "Bank error";
        break;
    case 49:
        result = "No need to fetch new data";
        break;
    case 50:
        result = "Memory leak";
        break;
    case 51:
        result = "The memory manager's block list has been corrupted";
        break;
    case 52:
        result = "Memory error";
        break;
    case 53:
        result = "The requested action was cancelled (not an error)";
        break;
    case 54:
        result = "Trying to load a bank using an ID which is not defined";
        break;
    case 55:
        result = "Asynchronous pipeline component is processing";
        break;
    case 56:
        result = "Error while reading a bank";
        break;
    case 57:
        result = "Invalid switch type (used with the switch container)";
        break;
    case 58:
        result = "Internal use only";
        break;
    case 59:
        result = "This environment is not defined";
        break;
    case 60:
        result = "This environment is used by an object";
        break;
    case 61:
        result = "This object is not defined";
        break;
    case 62:
        result = "Audio data already in target format, no conversion to perform";
        break;
    case 63:
        result = "Source format not known yet";
        break;
    case 64:
        result = "The bank version is not compatible with the current bank reader";
        break;
    case 65:
        result = "The provider has some data but does not process it (virtual voices)";
        break;
    case 66:
        result = "File not found";
        break;
    case 67:
        result = "IO device not ready";
        break;
    case 68:
        result = "The direct sound secondary buffer creation failed";
        break;
    case 69:
        result = "The bank load failed because the bank is already loaded";
        break;
    case 71:
        result = "The effect on the node is rendered";
        break;
    case 72:
        result = "A routine needs to be executed on some CPU";
        break;
    case 73:
        result = "The executed routine has finished its execution";
        break;
    case 74:
        result = "The memory manager should have been initialized at this point";
        break;
    case 75:
        result = "The stream manager should have been initialized at this point";
        break;
    case 76:
        result = "The machine does not support SSE instructions (required on PC)";
        break;
    case 77:
        result = "The system is busy and could not process the request";
        break;
    case 78:
        result = "Channel configuration is not supported in the current execution context";
        break;
    case 79:
        result = "Plugin media is not available for effect";
        break;
    case 80:
        result = "Sound was Not Allowed to play";
        break;
    case 81:
        result = "SDK command is too large to fit in the command queue";
        break;
    case 82:
        result = "A play request was rejected due to the MIDI filter parameters";
        break;
    default:
        result = "unknown";
        break;
    }

    std::cout << result << std::endl;
    return g_error_reason_msg(code);
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {

        /*FILE* console_stream = nullptr;

        AllocConsole();
        SetConsoleTitle(L"Warhammer 2 SNED (Script Native Extension DLL) console");
        freopen_s(&console_stream, "conout$", "w", stdout);*/

        bool bSpawnConsole = std::getenv("WARHAMMER_2_SNED_INLINE_CONSOLE") != nullptr;
        if (bSpawnConsole)
        {
            CreateNewConsole();
            SetConsoleTitle(L"Warhammer 2 SNED (Script Native Extension DLL) console");
        }
        else
        {
            AttachParentConsole();
        }

        std::cout << LicenseText << std::endl;

        if (MH_Initialize() != MH_OK)
        {
            std::cout << "Failed to initialize hook library." << std::endl;

            return 0;
        }

        HMODULE Warhammer2ExeAddress = GetModuleHandleA("Warhammer2.exe");
        if (!Warhammer2ExeAddress)
        {
            printf("failed to acquire Warhammer2 base");
            return false;
        }
        void* luaopen_packageAddress = (void*)GetProcAddress(Warhammer2ExeAddress, "luaopen_package");
        bool bAlternateMethod = false;
        if (!luaopen_packageAddress)
        {
            printf("failed to acquire position of luaopen_package. Maybe function is not exported? trying another attempt....");
            luaopen_packageAddress = (void*)(GetModuleHandleA("Warhammer2.exe") + 0x39d410);
            if (luaopen_packageAddress)
            {
                printf("hopefully second entry point is not dud. ++ Finger Crossed ++");
                bAlternateMethod = true;
            }
        }

        if (MH_CreateHook(luaopen_packageAddress, &hf_luaopen_package, reinterpret_cast<void**>(&g_fp_luaopen_package)) != MH_OK)
        {
            std::cout << "Failed to create `luaopen_package` hook." << std::endl;

            return 0;
        }
        
        void* lual_errorAddress = (void*)GetProcAddress(Warhammer2ExeAddress, "luaL_error");
        if (!lual_errorAddress)
        {
            std::cout << "Failed to find `lual_error`." << std::endl;

            return 0;
        }

        if(MH_CreateHook(lual_errorAddress, &hf_luaL_error, reinterpret_cast<void**>(&g_lual_error)) != MH_OK)
        {
            std::cout << "Failed to create `lual_error` hook." << std::endl;

            return 0;
        }
        ////hook engine output
        //{
        //    void* print_error_engine_original_Address = (void*)(GetModuleHandleA("Warhammer2.exe") + 0x388ee0);
        //    if (print_error_engine_original_Address)
        //    {
        //        std::cout << "attempting to hook into game engine logger..." << std::endl;
        //    }
        //    if (MH_CreateHook(print_error_engine_original_Address, EngineLogger, reinterpret_cast<void**>(&g_print_engine_something_original)) != MH_OK)
        //    {
        //        std::cout << "Failed to create `print_engine_something` hook." << std::endl;
        //    }
        //}
        ////hook engine output2
        //{
        //    void* print_error_engine_original_Address = (void*)(GetModuleHandleA("Warhammer2.exe") + 0x3884e0);
        //    if (print_error_engine_original_Address)
        //    {
        //        std::cout << "attempting to hook into game engine logger 2..." << std::endl;
        //    }
        //    if (MH_CreateHook(print_error_engine_original_Address, EngineLogger2, reinterpret_cast<void**>(&g_print_engine_something_2_original)) != MH_OK)
        //    {
        //        std::cout << "Failed to create `print_engine_something_2` hook." << std::endl;
        //    }
        //}
        ////hook error reason call
        //{
        //    void* error_reason = (void*)(GetModuleHandleA("Warhammer2.exe") + 0x2465c40);
        //    if (error_reason)
        //    {
        //        std::cout << "attempting to hook into ErrorReasonMsg..." << std::endl;
        //    }
        //    if (MH_CreateHook(error_reason, ErrorReasonMsg, reinterpret_cast<void**>(&g_error_reason_msg)) != MH_OK)
        //    {
        //        std::cout << "Failed to create `ErrorReasonMsg` hook." << std::endl;
        //    }
        //}
        
        
        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
        {
            std::cout << "Failed to enable hooks." << std::endl;

            return 0;
        }

        std::cout << "Everything seems normal." << std::endl;
        if (bAlternateMethod)
        {
            std::cout << "Although alternate method was used..." << std::endl;
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


