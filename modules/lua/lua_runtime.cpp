/**************************************************************************/
/*  lua_runtime.cpp                                                             */
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

#include "lua_runtime.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "scene/3d/node_3d.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
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

LuaRuntime *_runtime(lua_State *p_state) {
	return *static_cast<LuaRuntime **>(lua_getextraspace(p_state));
}

int _nexo_select_node(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	const char *path = luaL_checkstring(p_state, 1);
	if (!runtime || !runtime->select_node(NodePath(String::utf8(path)))) {
		return luaL_error(p_state, "Nexo.node.select: node path was not found");
	}
	return 0;
}

int _nexo_set_position(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) {
		return luaL_error(p_state, "Nexo.node.set_position requires a selected Node3D");
	}
	Vector3 position((real_t)luaL_checknumber(p_state, 1), (real_t)luaL_checknumber(p_state, 2), (real_t)luaL_checknumber(p_state, 3));
	node->set_position(position);
	return 0;
}

int _nexo_translate(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) {
		return luaL_error(p_state, "Nexo.node.translate requires a selected Node3D");
	}
	node->translate(Vector3((real_t)luaL_checknumber(p_state, 1), (real_t)luaL_checknumber(p_state, 2), (real_t)luaL_checknumber(p_state, 3)));
	return 0;
}

int _nexo_rotate_y(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) {
		return luaL_error(p_state, "Nexo.node.rotate_y requires a selected Node3D");
	}
	node->rotate_y((real_t)luaL_checknumber(p_state, 1));
	return 0;
}

int _nexo_set_visible(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) {
		return luaL_error(p_state, "Nexo.node.set_visible requires a selected Node3D");
	}
	node->set_visible(lua_toboolean(p_state, 1) != 0);
	return 0;
}

int _nexo_get_position(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) {
		return luaL_error(p_state, "Nexo.node.get_position requires a selected Node3D");
	}
	const Vector3 position = node->get_position();
	lua_pushnumber(p_state, position.x);
	lua_pushnumber(p_state, position.y);
	lua_pushnumber(p_state, position.z);
	return 3;
}

int _nexo_add_scene(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node *parent = runtime ? runtime->get_target_node() : nullptr;
	const char *raw_path = luaL_checkstring(p_state, 1);
	if (!parent) {
		return luaL_error(p_state, "Nexo.scene.add_child requires a selected parent node");
	}
	Ref<PackedScene> packed = ResourceLoader::load(String::utf8(raw_path), "PackedScene");
	if (packed.is_null()) return luaL_error(p_state, "Nexo.scene.add_child could not load PackedScene");
	Node *instance = packed->instantiate();
	if (!instance) {
		return luaL_error(p_state, "Nexo.scene.add_child failed to instantiate scene");
	}
	parent->add_child(instance);
	lua_pushstring(p_state, instance->get_name().utf8().get_data());
	return 1;
}

int _nexo_change_scene(lua_State *p_state) {
	const char *raw_path = luaL_checkstring(p_state, 1);
	LuaRuntime *runtime = _runtime(p_state);
	Node *target = runtime ? runtime->get_target_node() : nullptr;
	SceneTree *tree = target ? target->get_tree() : nullptr;
	if (!tree) {
		return luaL_error(p_state, "Nexo.scene.change requires an active SceneTree");
	}
	Ref<PackedScene> packed = ResourceLoader::load(String::utf8(raw_path), "PackedScene");
	if (packed.is_null()) return luaL_error(p_state, "Nexo.scene.change could not load PackedScene");
	const Error err = tree->change_scene_to_packed(packed);
	if (err != OK) {
		return luaL_error(p_state, "Nexo.scene.change failed with error %d", (int)err);
	}
	return 0;
}

int _nexo_signal_emit(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node *node = runtime ? runtime->get_target_node() : nullptr;
	const char *raw_name = luaL_checkstring(p_state, 1);
	if (!node) return luaL_error(p_state, "Nexo.signal.emit requires a live target Node");
	const StringName signal_name(String::utf8(raw_name));
	if (!node->has_signal(signal_name)) return luaL_error(p_state, "Nexo.signal.emit: signal is not declared on the target Node");
	Array args;
	args.push_back(signal_name);
	for (int i = 2; i <= lua_gettop(p_state); i++) {
		switch (lua_type(p_state, i)) {
			case LUA_TNIL: args.push_back(Variant()); break;
			case LUA_TBOOLEAN: args.push_back(lua_toboolean(p_state, i) != 0); break;
			case LUA_TNUMBER:
				if (lua_isinteger(p_state, i)) args.push_back((int64_t)lua_tointeger(p_state, i));
				else args.push_back((double)lua_tonumber(p_state, i));
				break;
			case LUA_TSTRING: args.push_back(String::utf8(lua_tostring(p_state, i))); break;
			case LUA_TTABLE: {
				const int table = lua_absindex(p_state, i);
				lua_getfield(p_state, table, "x");
				const bool has_x = !lua_isnil(p_state, -1);
				const double x = lua_tonumber(p_state, -1);
				lua_pop(p_state, 1);
				lua_getfield(p_state, table, "y");
				const bool has_y = !lua_isnil(p_state, -1);
				const double y = lua_tonumber(p_state, -1);
				lua_pop(p_state, 1);
				lua_getfield(p_state, table, "z");
				const bool has_z = !lua_isnil(p_state, -1);
				const double z = lua_tonumber(p_state, -1);
				lua_pop(p_state, 1);
				if (has_x && has_y && has_z) args.push_back(Vector3((real_t)x, (real_t)y, (real_t)z));
				else if (has_x && has_y) args.push_back(Vector2((real_t)x, (real_t)y));
				else args.push_back(Variant());
			} break;
			default: args.push_back(Variant()); break;
		}
	}
	const Variant emitted = node->callv("emit_signal", args);
	if (emitted.get_type() == Variant::INT && (int)emitted != (int)OK) {
		return luaL_error(p_state, "Nexo.signal.emit failed to emit signal");
	}
	return 0;
}

int _nexo_log(lua_State *p_state) {
	String message;
	const int count = lua_gettop(p_state);
	for (int i = 1; i <= count; i++) {
		size_t length = 0;
		const char *value = luaL_tolstring(p_state, i, &length);
		if (i > 1) {
			message += "  ";
		}
		if (value) {
			message += String::utf8(value, (int)length);
		}
		lua_pop(p_state, 1);
	}
	print_line(String("[Lua] ") + message);
	return 0;
}

String _stack_error(lua_State *p_state) {
	const char *message = lua_tostring(p_state, -1);
	return message ? String::utf8(message) : String("Unknown Lua error");
}
} // namespace

void LuaRuntime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("check_syntax", "source", "source_name"), &LuaRuntime::check_syntax, DEFVAL("res://script.lua"));
	ClassDB::bind_method(D_METHOD("execute_string", "source", "source_name"), &LuaRuntime::execute_string, DEFVAL("res://script.lua"));
	ClassDB::bind_method(D_METHOD("check_file", "path"), &LuaRuntime::check_file);
	ClassDB::bind_method(D_METHOD("execute_file", "path"), &LuaRuntime::execute_file);
	ClassDB::bind_method(D_METHOD("call_function", "name", "delta", "pass_delta"), &LuaRuntime::call_function, DEFVAL(0.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("call_function_args", "name", "args"), &LuaRuntime::call_function_args);
	ClassDB::bind_method(D_METHOD("get_global_value", "name"), &LuaRuntime::get_global_value);
	ClassDB::bind_method(D_METHOD("set_global_value", "name", "value"), &LuaRuntime::set_global_value);
	ClassDB::bind_method(D_METHOD("reset"), &LuaRuntime::reset);
	ClassDB::bind_method(D_METHOD("set_target_node", "node"), &LuaRuntime::set_target_node);
	ClassDB::bind_method(D_METHOD("get_target_node"), &LuaRuntime::get_target_node);
	ClassDB::bind_method(D_METHOD("select_node", "path"), &LuaRuntime::select_node);
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

	// A deliberately small engine bridge; add new capabilities only through reviewed bindings.
	lua_newtable(state);
	lua_pushcfunction(state, _nexo_log);
	lua_setfield(state, -2, "log");

	lua_newtable(state);
	lua_pushcfunction(state, _nexo_select_node);
	lua_setfield(state, -2, "select");
	lua_pushcfunction(state, _nexo_set_position);
	lua_setfield(state, -2, "set_position");
	lua_pushcfunction(state, _nexo_translate);
	lua_setfield(state, -2, "translate");
	lua_pushcfunction(state, _nexo_rotate_y);
	lua_setfield(state, -2, "rotate_y");
	lua_pushcfunction(state, _nexo_set_visible);
	lua_setfield(state, -2, "set_visible");
	lua_pushcfunction(state, _nexo_get_position);
	lua_setfield(state, -2, "get_position");
	lua_setfield(state, -2, "node");

	lua_newtable(state);
	lua_pushcfunction(state, _nexo_add_scene);
	lua_setfield(state, -2, "add_child");
	lua_pushcfunction(state, _nexo_change_scene);
	lua_setfield(state, -2, "change");
	lua_setfield(state, -2, "scene");

	lua_newtable(state);
	lua_pushcfunction(state, _nexo_signal_emit);
	lua_setfield(state, -2, "emit");
	lua_setfield(state, -2, "signal");
	lua_setglobal(state, "Nexo");

	*static_cast<LuaRuntime **>(lua_getextraspace(state)) = this;
}

void LuaRuntime::_destroy_state() {
	if (state) {
		lua_close(state);
		state = nullptr;
	}
}

void LuaRuntime::_instruction_hook(lua_State *p_state, lua_Debug *p_debug) {
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
	last_source_name = p_source_name;
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
		const int line = _line_from_error(error, p_source_name);
		ERR_PRINT(vformat("Lua runtime error in %s:%d: %s", p_source_name, line, error));
		lua_settop(state, 0);
		return _result(false, error, line, "runtime");
	}
	lua_settop(state, 0);
	return _result(true);
}


Dictionary LuaRuntime::check_file(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return _result(false, "Unable to open Lua file: " + p_path, -1, "io");
	}
	return check_syntax(file->get_as_text(), p_path);
}

Dictionary LuaRuntime::execute_file(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return _result(false, "Unable to open Lua file: " + p_path, -1, "io");
	}
	return execute_string(file->get_as_text(), p_path);
}

static Variant _lua_to_variant(lua_State *p_state, int p_index, int p_depth = 0) {
	if (p_depth > 16) return Variant();
	const int type = lua_type(p_state, p_index);
	switch (type) {
		case LUA_TNIL: return Variant();
		case LUA_TBOOLEAN: return Variant((bool)(lua_toboolean(p_state, p_index) != 0));
		case LUA_TNUMBER:
			if (lua_isinteger(p_state, p_index)) return Variant((int64_t)lua_tointeger(p_state, p_index));
			return Variant((double)lua_tonumber(p_state, p_index));
		case LUA_TSTRING: {
			size_t length = 0;
			const char *value = lua_tolstring(p_state, p_index, &length);
			return Variant(String::utf8(value, (int)length));
		}
		case LUA_TTABLE: {
			const int absolute = lua_absindex(p_state, p_index);
			lua_getfield(p_state, absolute, "x");
			const bool has_x = !lua_isnil(p_state, -1);
			const double x = lua_tonumber(p_state, -1);
			lua_pop(p_state, 1);
			lua_getfield(p_state, absolute, "y");
			const bool has_y = !lua_isnil(p_state, -1);
			const double y = lua_tonumber(p_state, -1);
			lua_pop(p_state, 1);
			lua_getfield(p_state, absolute, "z");
			const bool has_z = !lua_isnil(p_state, -1);
			const double z = lua_tonumber(p_state, -1);
			lua_pop(p_state, 1);
			if (has_x && has_y && has_z) return Variant(Vector3((real_t)x, (real_t)y, (real_t)z));
			if (has_x && has_y && lua_gettop(p_state) >= 0) return Variant(Vector2((real_t)x, (real_t)y));
			Array array;
			Dictionary dictionary;
			bool is_array = true;
			int count = 0;
			lua_pushnil(p_state);
			while (lua_next(p_state, absolute) != 0) {
				count++;
				if (!lua_isinteger(p_state, -2) || lua_tointeger(p_state, -2) < 1) is_array = false;
				lua_pop(p_state, 1);
			}
			if (is_array) {
				for (int i = 1; i <= count; i++) {
					lua_rawgeti(p_state, absolute, i);
					array.push_back(_lua_to_variant(p_state, -1, p_depth + 1));
					lua_pop(p_state, 1);
				}
				return Variant(array);
			}
			lua_pushnil(p_state);
			while (lua_next(p_state, absolute) != 0) {
				Variant key = _lua_to_variant(p_state, -2, p_depth + 1);
				Variant value = _lua_to_variant(p_state, -1, p_depth + 1);
				if (key.get_type() != Variant::NIL) dictionary[key] = value;
				lua_pop(p_state, 1);
			}
			return Variant(dictionary);
		}
		default: return Variant();
	}
}

static void _push_variant_to_lua(lua_State *p_state, const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::NIL:
			lua_pushnil(p_state);
			break;
		case Variant::BOOL:
			lua_pushboolean(p_state, (bool)p_value);
			break;
		case Variant::INT:
			lua_pushinteger(p_state, (lua_Integer)(int64_t)p_value);
			break;
		case Variant::FLOAT:
			lua_pushnumber(p_state, (lua_Number)(double)p_value);
			break;
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH: {
			CharString value = p_value.stringify().utf8();
			lua_pushlstring(p_state, value.get_data(), value.length());
		} break;
		case Variant::VECTOR2I: {
			Vector2i value = p_value;
			lua_createtable(p_state, 0, 2);
			lua_pushinteger(p_state, value.x); lua_setfield(p_state, -2, "x");
			lua_pushinteger(p_state, value.y); lua_setfield(p_state, -2, "y");
		} break;
		case Variant::VECTOR3I: {
			Vector3i value = p_value;
			lua_createtable(p_state, 0, 3);
			lua_pushinteger(p_state, value.x); lua_setfield(p_state, -2, "x");
			lua_pushinteger(p_state, value.y); lua_setfield(p_state, -2, "y");
			lua_pushinteger(p_state, value.z); lua_setfield(p_state, -2, "z");
		} break;
		case Variant::VECTOR4: {
			Vector4 value = p_value;
			lua_createtable(p_state, 0, 4);
			lua_pushnumber(p_state, value.x); lua_setfield(p_state, -2, "x");
			lua_pushnumber(p_state, value.y); lua_setfield(p_state, -2, "y");
			lua_pushnumber(p_state, value.z); lua_setfield(p_state, -2, "z");
			lua_pushnumber(p_state, value.w); lua_setfield(p_state, -2, "w");
		} break;
		case Variant::COLOR: {
			Color value = p_value;
			lua_createtable(p_state, 0, 4);
			lua_pushnumber(p_state, value.r); lua_setfield(p_state, -2, "r");
			lua_pushnumber(p_state, value.g); lua_setfield(p_state, -2, "g");
			lua_pushnumber(p_state, value.b); lua_setfield(p_state, -2, "b");
			lua_pushnumber(p_state, value.a); lua_setfield(p_state, -2, "a");
		} break;
		case Variant::ARRAY: {
			Array value = p_value;
			lua_createtable(p_state, value.size(), 0);
			for (int i = 0; i < value.size(); i++) {
				_push_variant_to_lua(p_state, value[i]);
				lua_rawseti(p_state, -2, i + 1);
			}
		} break;
		case Variant::DICTIONARY: {
			Dictionary value = p_value;
			Array keys = value.keys();
			lua_createtable(p_state, 0, keys.size());
			for (int i = 0; i < keys.size(); i++) {
				Variant key = keys[i];
				if (key.get_type() != Variant::STRING && key.get_type() != Variant::STRING_NAME && key.get_type() != Variant::INT) continue;
				_push_variant_to_lua(p_state, key);
				_push_variant_to_lua(p_state, value[key]);
				lua_settable(p_state, -3);
			}
		} break;
		case Variant::VECTOR2: {
			Vector2 value = p_value;
			lua_createtable(p_state, 0, 2);
			lua_pushnumber(p_state, value.x);
			lua_setfield(p_state, -2, "x");
			lua_pushnumber(p_state, value.y);
			lua_setfield(p_state, -2, "y");
		} break;
		case Variant::VECTOR3: {
			Vector3 value = p_value;
			lua_createtable(p_state, 0, 3);
			lua_pushnumber(p_state, value.x);
			lua_setfield(p_state, -2, "x");
			lua_pushnumber(p_state, value.y);
			lua_setfield(p_state, -2, "y");
			lua_pushnumber(p_state, value.z);
			lua_setfield(p_state, -2, "z");
		} break;
		default:
			// Unsupported engine objects are represented as nil until safe userdata
			// wrappers with lifetime tracking are implemented.
			lua_pushnil(p_state);
			break;
	}
}

Variant LuaRuntime::get_global_value(const String &p_name) {
	if (!state) return Variant();
	CharString name = p_name.utf8();
	lua_getglobal(state, name.get_data());
	Variant value = _lua_to_variant(state, -1);
	lua_pop(state, 1);
	return value;
}

void LuaRuntime::set_global_value(const String &p_name, const Variant &p_value) {
	if (!state) return;
	CharString name = p_name.utf8();
	_push_variant_to_lua(state, p_value);
	lua_setglobal(state, name.get_data());
}

Dictionary LuaRuntime::call_function_args(const String &p_name, const Array &p_args) {
	if (!state) {
		return _result(false, "Could not create Lua state", -1, "runtime");
	}
	CharString function_name = p_name.utf8();
	lua_getglobal(state, function_name.get_data());
	if (!lua_isfunction(state, -1)) {
		lua_pop(state, 1);
		return _result(true);
	}
	for (int i = 0; i < p_args.size(); i++) {
		_push_variant_to_lua(state, p_args[i]);
	}
	remaining_instructions = instruction_budget;
	lua_sethook(state, _instruction_hook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
	const int status = lua_pcall(state, p_args.size(), 1, 0);
	lua_sethook(state, nullptr, 0, 0);
	if (status != LUA_OK) {
		const String error = _stack_error(state);
		const int line = _line_from_error(error, last_source_name);
		ERR_PRINT(vformat("Lua callback '%s' failed: %s", p_name, error));
		lua_settop(state, 0);
		return _result(false, error, line, "runtime");
	}
	Variant returned_value = _lua_to_variant(state, -1);
	lua_settop(state, 0);
	Dictionary result = _result(true);
	result["value"] = returned_value;
	return result;
}

Dictionary LuaRuntime::call_function(const String &p_name, double p_delta, bool p_pass_delta) {
	if (!state) {
		return _result(false, "Could not create Lua state", -1, "runtime");
	}
	CharString function_name = p_name.utf8();
	lua_getglobal(state, function_name.get_data());
	if (!lua_isfunction(state, -1)) {
		lua_pop(state, 1);
		return _result(true);
	}
	int argument_count = 0;
	if (p_pass_delta) {
		lua_pushnumber(state, p_delta);
		argument_count = 1;
	}
	remaining_instructions = instruction_budget;
	lua_sethook(state, _instruction_hook, LUA_MASKCOUNT, LUA_HOOK_GRANULARITY);
	const int status = lua_pcall(state, argument_count, 0, 0);
	lua_sethook(state, nullptr, 0, 0);
	if (status != LUA_OK) {
		const String error = _stack_error(state);
		const int line = _line_from_error(error, last_source_name);
		ERR_PRINT(vformat("Lua callback '%s' failed: %s", p_name, error));
		lua_settop(state, 0);
		return _result(false, error, line, "runtime");
	}
	lua_settop(state, 0);
	return _result(true);
}

void LuaRuntime::set_target_node(Node *p_node) {
	target_node_id = p_node ? p_node->get_instance_id() : ObjectID();
}

Node *LuaRuntime::get_target_node() const {
	return Object::cast_to<Node>(ObjectDB::get_instance(target_node_id));
}

bool LuaRuntime::select_node(const NodePath &p_path) {
	Node *root = get_target_node();
	if (!root) return false;
	Node *selected = root->get_node_or_null(p_path);
	if (!selected) return false;
	target_node_id = selected->get_instance_id();
	return true;
}

void LuaRuntime::reset() {
	_create_state();
}

void LuaRuntime::set_instruction_budget(int p_instructions) {
	instruction_budget = CLAMP(p_instructions, 1000, 100000000);
}
