/**************************************************************************/
/*  lua_script_runner.cpp                                                       */
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

#include "lua_script_runner.h"

#include "core/object/class_db.h"
#include "core/error/error_macros.h"

void LuaScriptRunner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_script_path", "path"), &LuaScriptRunner::set_script_path);
	ClassDB::bind_method(D_METHOD("get_script_path"), &LuaScriptRunner::get_script_path);
	ClassDB::bind_method(D_METHOD("get_runtime"), &LuaScriptRunner::get_runtime);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_path", PROPERTY_HINT_FILE, "*.lua"), "set_script_path", "get_script_path");
}

LuaScriptRunner::LuaScriptRunner() {
	runtime.instantiate();
}

void LuaScriptRunner::set_script_path(const String &p_path) {
	if (script_path == p_path) {
		return;
	}
	script_path = p_path;
	script_loaded = false;
}

String LuaScriptRunner::get_script_path() const {
	return script_path;
}

void LuaScriptRunner::_ready() {
	if (script_path.is_empty()) {
		ERR_PRINT("LuaScriptRunner requires a .lua script path.");
		return;
	}
	if (script_path.get_extension().to_lower() != "lua") {
		ERR_PRINT(vformat("LuaScriptRunner only accepts .lua files, got: %s", script_path));
		return;
	}

	// Bind gameplay APIs to the parent scene node when present. This lets a Lua
	// runner child control its Node3D parent without a GDScript adapter.
	runtime->set_target_node(get_parent() ? get_parent() : this);
	Dictionary result = runtime->execute_file(script_path);
	if (!bool(result.get("ok", false))) {
		ERR_PRINT(vformat("Unable to load Lua script '%s': %s", script_path, String(result.get("error", "Unknown error"))));
		return;
	}

	script_loaded = true;
	set_process(true);
	Dictionary ready_result = runtime->call_function("_ready");
	if (!bool(ready_result.get("ok", false))) {
		script_loaded = false;
		set_process(false);
	}
}

void LuaScriptRunner::_process(double p_delta) {
	if (script_loaded) {
		Dictionary process_result = runtime->call_function("_process", p_delta, true);
		if (!bool(process_result.get("ok", false))) {
			script_loaded = false;
			set_process(false);
		}
	}
}
