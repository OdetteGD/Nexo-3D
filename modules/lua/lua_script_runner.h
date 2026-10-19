/**************************************************************************/
/*  lua_script_runner.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
#pragma once

#include "lua_runtime.h"
#include "scene/main/node.h"

class LuaScriptRunner : public Node {
	GDCLASS(LuaScriptRunner, Node);

	String script_path = "res://main.lua";
	Ref<LuaRuntime> runtime;
	bool script_loaded = false;

protected:
	static void _bind_methods();

public:
	void set_script_path(const String &p_path);
	String get_script_path() const;
	Ref<LuaRuntime> get_runtime() const { return runtime; }

	void _ready() override;
	void _process(double p_delta) override;

	LuaScriptRunner();
};
