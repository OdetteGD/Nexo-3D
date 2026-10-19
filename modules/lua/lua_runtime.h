#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"

extern "C" {
#include "lua.h"
}

class LuaRuntime : public RefCounted {
    GDCLASS(LuaRuntime, RefCounted);

    lua_State *state = nullptr;
    int instruction_budget = 1000000;
    int remaining_instructions = 1000000;

    static void _instruction_hook(lua_State *p_state, lua_Debug *p_debug);
    void _create_state();
    void _destroy_state();

protected:
    static void _bind_methods();

public:
    // Results contain: ok, error, line, and kind ("syntax" or "runtime").
    Dictionary check_syntax(const String &p_source, const String &p_source_name = "res://script.lua");
    Dictionary check_file(const String &p_path);
    Dictionary execute_file(const String &p_path);
    Dictionary execute_string(const String &p_source, const String &p_source_name = "res://script.lua");
    void reset();
    void set_instruction_budget(int p_instructions);
    int get_instruction_budget() const { return instruction_budget; }

    LuaRuntime();
    ~LuaRuntime();
};
