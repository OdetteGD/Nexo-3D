/**************************************************************************/
/*  lua_script.cpp                                                        */
/**************************************************************************/
#include "lua_script.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "scene/main/node.h"

LuaScriptLanguage *LuaScriptLanguage::singleton = nullptr;

LuaScript::LuaScript() {
	source_code = "-- Lua script attached directly to a Node.\n\nfunction _ready()\n    Nexo.log(\"Lua script ready\")\nend\n\nfunction _process(delta)\nend\n";
	reload();
}

void LuaScript::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_source_code", "source"), &LuaScript::set_source_code);
	ClassDB::bind_method(D_METHOD("get_source_code"), &LuaScript::get_source_code);
	ClassDB::bind_method(D_METHOD("reload", "keep_state"), &LuaScript::reload, DEFVAL(false));
}

void LuaScript::set_source_code(const String &p_code) {
	source_code = p_code;
	valid = false;
}

Error LuaScript::reload(bool p_keep_state) {
	Ref<LuaRuntime> runtime;
	runtime.instantiate();
	Dictionary result = runtime->check_syntax(source_code, get_path().is_empty() ? "res://script.lua" : get_path());
	valid = bool(result.get("ok", false));
	if (!valid) {
		ERR_PRINT(vformat("Lua script syntax error in %s: %s", get_path(), String(result.get("error", "unknown error"))));
		return ERR_PARSE_ERROR;
	}
	return OK;
}

ScriptInstance *LuaScript::instance_create(Object *p_this) {
	if (!valid || !p_this || !Object::cast_to<Node>(p_this)) {
		return nullptr;
	}
	return memnew(LuaScriptInstance(this, p_this));
}

bool LuaScript::has_method(const StringName &p_method) const {
	return p_method == SNAME("_ready") || p_method == SNAME("_process") ||
			p_method == SNAME("_enter_tree") || p_method == SNAME("_exit_tree") ||
			p_method == SNAME("_physics_process") || p_method == SNAME("_input") ||
			p_method == SNAME("_unhandled_input") || p_method == SNAME("_notification");
}

MethodInfo LuaScript::get_method_info(const StringName &p_method) const {
	MethodInfo info;
	info.name = p_method;
	if (p_method == SNAME("_process") || p_method == SNAME("_physics_process")) {
		info.arguments.push_back(PropertyInfo(Variant::FLOAT, "delta"));
	}
	return info;
}

void LuaScript::get_script_method_list(List<MethodInfo> *r_list) const {
	static const char *callbacks[] = { "_enter_tree", "_ready", "_process", "_physics_process", "_input", "_unhandled_input", "_exit_tree" };
	for (const char *callback : callbacks) {
		r_list->push_back(get_method_info(StringName(callback)));
	}
}

ScriptLanguage *LuaScript::get_language() const {
	return LuaScriptLanguage::get_singleton();
}

LuaScriptLanguage::LuaScriptLanguage() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

LuaScriptLanguage::~LuaScriptLanguage() {
	if (singleton == this) singleton = nullptr;
}

Vector<String> LuaScriptLanguage::get_reserved_words() const {
	return Vector<String>{ "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while" };
}

bool LuaScriptLanguage::is_control_flow_keyword(const String &p_string) const {
	return p_string == "if" || p_string == "elseif" || p_string == "else" || p_string == "for" || p_string == "while" || p_string == "repeat" || p_string == "until" || p_string == "do";
}

Vector<String> LuaScriptLanguage::get_comment_delimiters() const {
	return Vector<String>{ "--" };
}

Vector<String> LuaScriptLanguage::get_doc_comment_delimiters() const {
	return Vector<String>{ "---" };
}

Vector<String> LuaScriptLanguage::get_string_delimiters() const {
	return Vector<String>{ "\"\"", "''", "[[ ]]" };
}

Ref<Script> LuaScriptLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<LuaScript> script;
	script.instantiate();
	const String class_name = p_class_name.is_empty() ? String("NewLuaScript") : p_class_name;
	script->set_source_code("-- " + class_name + ".lua\n\nfunction _ready()\n    Nexo.log(\"" + class_name + " ready\")\nend\n\nfunction _process(delta)\nend\n");
	script->set_path("res://" + class_name.to_snake_case() + ".lua");
	script->reload();
	return script;
}

String LuaScriptLanguage::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const {
	String args;
	for (int i = 0; i < p_args.size(); i++) {
		if (i > 0) args += ", ";
		args += p_args[i];
	}
	return "\nfunction " + p_name + "(" + args + ")\n    -- TODO\nend\n";
}

LuaScriptInstance::LuaScriptInstance(LuaScript *p_script, Object *p_owner) {
	script = Ref<LuaScript>(p_script);
	owner = p_owner;
	runtime.instantiate();
	runtime->set_target_node(Object::cast_to<Node>(owner));
	Dictionary result = runtime->execute_string(script->get_source_code(), script->get_path().is_empty() ? "res://script.lua" : script->get_path());
	loaded = bool(result.get("ok", false));
	if (!loaded) {
		ERR_PRINT(vformat("Could not execute attached Lua script: %s", String(result.get("error", "unknown error"))));
	}
}

void LuaScriptInstance::get_method_list(List<MethodInfo> *r_list) const {
	if (!script.is_null()) script->get_script_method_list(r_list);
}

bool LuaScriptInstance::has_method(const StringName &p_method) const {
	return !script.is_null() && script->has_method(p_method);
}

Variant LuaScriptInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	if (!loaded || runtime.is_null()) return Variant();
	Dictionary result;
	if (p_method == SNAME("_process") || p_method == SNAME("_physics_process")) {
		const double delta = p_argcount > 0 && p_args[0] ? (double)*p_args[0] : 0.0;
		result = runtime->call_function(String(p_method), delta, true);
	} else {
		result = runtime->call_function(String(p_method));
	}
	if (!bool(result.get("ok", false))) r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void LuaScriptInstance::notification(int p_notification, bool p_reversed) {
	if (!loaded || !runtime.is_valid()) return;
	Node *node = Object::cast_to<Node>(owner);
	if (p_notification == Node::NOTIFICATION_READY) {
		runtime->call_function("_ready");
	} else if (p_notification == Node::NOTIFICATION_PROCESS) {
		runtime->call_function("_process", node ? node->get_process_delta_time() : 0.0, true);
	} else if (p_notification == Node::NOTIFICATION_PHYSICS_PROCESS) {
		runtime->call_function("_physics_process", node ? node->get_physics_process_delta_time() : 0.0, true);
	} else if (p_notification == Node::NOTIFICATION_ENTER_TREE) {
		runtime->call_function("_enter_tree");
	} else if (p_notification == Node::NOTIFICATION_EXIT_TREE) {
		runtime->call_function("_exit_tree");
	}
}

ScriptLanguage *LuaScriptInstance::get_language() {
	return LuaScriptLanguage::get_singleton();
}
