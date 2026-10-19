#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    lua_State *state = luaL_newstate();
    if (state == NULL) {
        fputs("failed to create Lua state\n", stderr);
        return 1;
    }
    luaL_openlibs(state);

    const char *valid = "local total = 0\nfor i = 1, 5 do total = total + i end\nassert(total == 15)";
    if (luaL_loadbufferx(state, valid, strlen(valid), "@valid.lua", "t") != LUA_OK ||
            lua_pcall(state, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "valid fixture failed: %s\n", lua_tostring(state, -1));
        lua_close(state);
        return 2;
    }

    const char *invalid = "local = 'broken'";
    if (luaL_loadbufferx(state, invalid, strlen(invalid), "@invalid.lua", "t") == LUA_OK) {
        fputs("invalid fixture unexpectedly compiled\n", stderr);
        lua_close(state);
        return 3;
    }
    const char *diagnostic = lua_tostring(state, -1);
    if (diagnostic == NULL || strstr(diagnostic, "invalid.lua:1:") == NULL) {
        fprintf(stderr, "missing syntax diagnostic: %s\n", diagnostic ? diagnostic : "(null)");
        lua_close(state);
        return 4;
    }

    lua_close(state);
    puts("vendored Lua 5.4 runtime smoke test passed");
    return 0;
}
