#include "lua_runtime.h"

#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/string/ustring.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace {
constexpr int LUA_HOOK_GRANULARITY = 1000;

Dictionary _result(bool p_ok, const String &p_error = String(), int p_line = -1, const String &p_kind = String()) {
    Dictionary result;
    result["ok"] = p_ok;
    result["error"] = p_error;
    result["line"] = p_line;
    result["kind"] = p_kind;
    return result;
}

int _line_from_error(const String &p_error, const String &p_source_name) {
    const String prefix = p_source_name + ":";
    const int start = p_error.find(prefix);
    if (start < 0) {
        return -1;
    }
    const int number_start = start + prefix.length();
    const int colon = p_error.find(":", number_start);
    if (colon < 0) {
        return -1;
    }
    const String number = p_error.substr(number_start, colon - number_start);
    if (!number.is_valid_int()) {
        return -1;
    }
    return number.to_int();
}

String _stack_error(lua_State *p_state) {
    const char *message = lua_tostring(p_state, -1);
    return message ? String::utf8(message) : String("Unknown Lua error");
}
} // namespace

void LuaRuntime::_bind_methods() {
    ClassDB::bind_method(D_METHOD("check_syntax", "source", "source_name"), &LuaRuntime::check_syntax, DEFVAL("res://script.lua"));
    ClassDB::bind_method(D_METHOD("execute_string", "source", "source_name"), &LuaRuntime::execute_string, DEFVAL("res://script.lua"));
    ClassDB::bind_method(D_METHOD("reset"), &LuaRuntime::reset);
    ClassDB::bind_method(D_METHOD("set_instruction_budget", "instructions"), &LuaRuntime::set_instruction_budget);
    ClassDB::bind_method(D_METHOD("get_instruction_budget"), &LuaRuntime::get_instruction_budget);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "instruction_budget", PROPERTY_HINT_RANGE, "1000,100000000,1000"), "set_instruction_budget", "get_instruction_budget");
}

LuaRuntime::LuaRuntime() {
    _create_state();
}

LuaRuntime::~LuaRuntime() {
    _destroy_state();
}

void LuaRuntime::_create_state() {
    _destroy_state();
    state = luaL_newstate();
    if (!state) {
        return;
    }

    // Deliberately expose a small standard library set. No io, os, debug, or package.
    luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state, 1);
    luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(state, 1);

    // The base library contains file/dynamic loading helpers; remove them explicitly.
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");
    lua_pushnil(state);
    lua_setglobal(state, "load");
    lua_pushnil(state);
    lua_setglobal(state, "collectgarbage");
    *static_cast<LuaRuntime **>(lua_getextraspace(state)) = this;
}

void LuaRuntime::_destroy_state() {
    if (state) {
        lua_close(state);
        state = nullptr;
    }
}

void LuaRuntime::_instruction_hook(lua_State *p_state, void *p_debug) {
    (void)p_debug;
    LuaRuntime *runtime = *static_cast<LuaRuntime **>(lua_getextraspace(p_state));
    if (!runtime) {
        luaL_error(p_state, "Nexo Lua runtime budget exceeded");
        return;
    }
    runtime->remaining_instructions -= LUA_HOOK_GRANULARITY;
    if (runtime->remaining_instructions <= 0) {
        luaL_error(p_state, "Nexo Lua instruction budget exceeded");
    }
}

Dictionary LuaRuntime::check_syntax(const String &p_source, const String &p_source_name) {
    if (!state) {
        return _result(false, "Could not create Lua state", -1, "runtime");
    }
    CharString source = p_source.utf8();
    CharString name = p_source_name.utf8();
    const int status = luaL_loadbufferx(state, source.get_data(), source.length(), name.get_data(), "t");
    if (status == LUA_OK) {
        lua_pop(state, 1);
        return _result(true);
    }
    const String error = _stack_error(state);
    lua_pop(state, 1);
    return _result(false, error, _line_from_error(error, p_source_name), "syntax");
}

Dictionary LuaRuntime::execute_string(const String &p_source, const String &p_source_name) {
    Dictionary syntax = check_syntax(p_source, p_source_name);
    if (!bool(syntax["ok"])) {
        return syntax;
    }
    if (!state) {
        return _result(false, "Could not create Lua state", -1, "runtime");
    }

    CharString source = p_source.utf8();
    CharString name = p_source_name.utf8();
    const int load_status = luaL_loadbufferx(state, source.get_data(), source.length(), name.get_data(), "t");
    if (load_status != LUA_OK) {
        const String error = _stack_error(state);
        lua_pop(state, 1);
        return _result(false, error, _line_from_error(error, p_source_name), "syntax");
    }

    remaining_instructions = instruction_budget;
    lua_sethook(state, _instruction_hook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
    const int call_status = lua_pcall(state, 0, LUA_MULTRET, 0);
    lua_sethook(state, nullptr, 0, 0);
    if (call_status != LUA_OK) {
        const String error = _stack_error(state);
        lua_settop(state, 0);
        return _result(false, error, _line_from_error(error, p_source_name), "runtime");
    }
    lua_settop(state, 0);
    return _result(true);
}

void LuaRuntime::reset() {
    _create_state();
}

void LuaRuntime::set_instruction_budget(int p_instructions) {
    instruction_budget = CLAMP(p_instructions, 1000, 100000000);
}
