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

#include "lua_runtime.h"

#include "core/io/file_access.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/object/object_db.h"
#include "core/io/resource_loader.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"
#include "scene/3d/node_3d.h"
#include "core/string/ustring.h"
#include "core/string/print_string.h"

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
	if (!node) return luaL_error(p_state, "Nexo.node.set_position requires a selected Node3D");
	Vector3 position((real_t)luaL_checknumber(p_state, 1), (real_t)luaL_checknumber(p_state, 2), (real_t)luaL_checknumber(p_state, 3));
	node->set_position(position);
	return 0;
}

int _nexo_translate(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) return luaL_error(p_state, "Nexo.node.translate requires a selected Node3D");
	node->translate(Vector3((real_t)luaL_checknumber(p_state, 1), (real_t)luaL_checknumber(p_state, 2), (real_t)luaL_checknumber(p_state, 3)));
	return 0;
}

int _nexo_rotate_y(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) return luaL_error(p_state, "Nexo.node.rotate_y requires a selected Node3D");
	node->rotate_y((real_t)luaL_checknumber(p_state, 1));
	return 0;
}

int _nexo_set_visible(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) return luaL_error(p_state, "Nexo.node.set_visible requires a selected Node3D");
	node->set_visible(lua_toboolean(p_state, 1) != 0);
	return 0;
}

int _nexo_get_position(lua_State *p_state) {
	LuaRuntime *runtime = _runtime(p_state);
	Node3D *node = runtime ? Object::cast_to<Node3D>(runtime->get_target_node()) : nullptr;
	if (!node) return luaL_error(p_state, "Nexo.node.get_position requires a selected Node3D");
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
	if (!parent) return luaL_error(p_state, "Nexo.scene.add_child requires a selected parent node");
	Ref<PackedScene> packed = ResourceLoader::load(String::utf8(raw_path), "PackedScene");
	if (packed.is_null()) return luaL_error(p_state, "Nexo.scene.add_child could not load PackedScene");
	Node *instance = packed->instantiate();
	if (!instance) return luaL_error(p_state, "Nexo.scene.add_child failed to instantiate scene");
	parent->add_child(instance);
	lua_pushstring(p_state, instance->get_name().utf8().get_data());
	return 1;
}

int _nexo_change_scene(lua_State *p_state) {
	const char *raw_path = luaL_checkstring(p_state, 1);
	LuaRuntime *runtime = _runtime(p_state);
	Node *target = runtime ? runtime->get_target_node() : nullptr;
	SceneTree *tree = target ? target->get_tree() : nullptr;
	if (!tree) return luaL_error(p_state, "Nexo.scene.change requires an active SceneTree");
	Ref<PackedScene> packed = ResourceLoader::load(String::utf8(raw_path), "PackedScene");
	if (packed.is_null()) return luaL_error(p_state, "Nexo.scene.change could not load PackedScene");
	const Error err = tree->change_scene_to_packed(packed);
	if (err != OK) return luaL_error(p_state, "Nexo.scene.change failed with error %d", (int)err);
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
