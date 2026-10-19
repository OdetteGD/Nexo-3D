/**************************************************************************/
/*  runtime_smoke.c                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,     */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

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
