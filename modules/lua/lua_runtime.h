/**************************************************************************/
/*  lua_runtime.h                                                               */
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
/* without limitation the rights to use, copy, modify, merge, publish,    */
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

#pragma once

#include "core/object/ref_counted.h"
#include "core/variant/dictionary.h"
#include "core/object/object_id.h"

extern "C" {
#include "lua.h"
}

class Node;
class Node3D;

class LuaRuntime : public RefCounted {
	GDCLASS(LuaRuntime, RefCounted);

	lua_State *state = nullptr;
	int instruction_budget = 1000000;
	int remaining_instructions = 1000000;
	ObjectID target_node_id;
	String last_source_name = "res://script.lua";

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
	Dictionary call_function(const String &p_name, double p_delta = 0.0, bool p_pass_delta = false);
	Dictionary call_function_args(const String &p_name, const Array &p_args);
	Variant get_global_value(const String &p_name);
	void set_global_value(const String &p_name, const Variant &p_value);
	Dictionary execute_string(const String &p_source, const String &p_source_name = "res://script.lua");
	void reset();
	void set_target_node(Node *p_node);
	Node *get_target_node() const;
	bool select_node(const NodePath &p_path);
	void set_instruction_budget(int p_instructions);
	int get_instruction_budget() const { return instruction_budget; }

	LuaRuntime();
	~LuaRuntime();
};
