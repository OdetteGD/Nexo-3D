/**************************************************************************/
/*  lua_script.cpp                                                        */
/**************************************************************************/
#include "lua_script.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "scene/main/node.h"

LuaScriptLanguage *LuaScriptLanguage::singleton = nullptr;

static Array _lua_export_declarations(const String &p_source) {
	Array exports;
	const PackedStringArray lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (!line.begins_with("-- @export ")) continue;
		line = line.substr(String("-- @export ").length()).strip_edges();
		const int equal = line.find("=");
		String left = equal >= 0 ? line.substr(0, equal).strip_edges() : line;
		String initial = equal >= 0 ? line.substr(equal + 1).strip_edges() : String();
		const int colon = left.find(":");
		if (colon <= 0) continue;
		String name = left.substr(0, colon).strip_edges();
		String type_name = left.substr(colon + 1).strip_edges();
		if (name.is_empty() || name.contains(" ") || name.contains(".") || name.contains(":")) continue;
		Variant::Type type = Variant::NIL;
		if (type_name == "int") type = Variant::INT;
		else if (type_name == "float" || type_name == "number") type = Variant::FLOAT;
		else if (type_name == "bool" || type_name == "boolean") type = Variant::BOOL;
		else if (type_name == "string") type = Variant::STRING;
		else if (type_name == "Vector2") type = Variant::VECTOR2;
		else if (type_name == "Vector3") type = Variant::VECTOR3;
		else continue;
		Variant default_value;
		if (type == Variant::INT) default_value = initial.is_empty() ? Variant(0) : Variant((int64_t)initial.to_int());
		else if (type == Variant::FLOAT) default_value = initial.is_empty() ? Variant(0.0) : Variant(initial.to_float());
		else if (type == Variant::BOOL) default_value = initial == "true";
		else if (type == Variant::STRING) {
			if (initial.length() >= 2 && ((initial.begins_with("\"") && initial.ends_with("\"")) || (initial.begins_with("'") && initial.ends_with("'")))) initial = initial.substr(1, initial.length() - 2);
			default_value = initial;
		} else if (type == Variant::VECTOR2) default_value = Vector2();
		else if (type == Variant::VECTOR3) default_value = Vector3();
		Dictionary item;
		item["name"] = name;
		item["type"] = (int)type;
		item["default"] = default_value;
		exports.push_back(item);
	}
	return exports;
}

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
	if (p_method == SNAME("_ready") || p_method == SNAME("_process") ||
			p_method == SNAME("_enter_tree") || p_method == SNAME("_exit_tree") ||
			p_method == SNAME("_physics_process") || p_method == SNAME("_input") ||
			p_method == SNAME("_unhandled_input") || p_method == SNAME("_notification")) {
		return true;
	}
	const String declaration = "function " + String(p_method) + "(";
	const String local_declaration = "local function " + String(p_method) + "(";
	return source_code.contains(declaration) || source_code.contains(local_declaration);
}

MethodInfo LuaScript::get_method_info(const StringName &p_method) const {
	MethodInfo info;
	info.name = p_method;
	const String declaration = "function " + String(p_method) + "(";
	int start = source_code.find(declaration);
	if (start < 0) {
		if (p_method == SNAME("_process") || p_method == SNAME("_physics_process")) {
			info.arguments.push_back(PropertyInfo(Variant::FLOAT, "delta"));
		}
		return info;
	}
	start += declaration.length();
	const int end = source_code.find(")", start);
	if (end >= start) {
		PackedStringArray args = source_code.substr(start, end - start).split(",");
		for (int i = 0; i < args.size(); i++) {
			String arg = args[i].strip_edges();
			if (!arg.is_empty() && arg != "...") {
				info.arguments.push_back(PropertyInfo(Variant::NIL, arg));
			}
		}
	}
	return info;
}

void LuaScript::get_script_method_list(List<MethodInfo> *r_list) const {
	HashSet<StringName> added;
	const PackedStringArray lines = source_code.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		String declaration;
		if (line.begins_with("function ")) {
			declaration = line.substr(String("function ").length());
		} else if (line.begins_with("local function ")) {
			declaration = line.substr(String("local function ").length());
		}
		if (declaration.is_empty()) {
			continue;
		}
		const int paren = declaration.find("(");
		if (paren <= 0) {
			continue;
		}
		const String name = declaration.substr(0, paren).strip_edges();
		if (name.contains(".") || name.contains(":")) {
			continue;
		}
		const StringName method_name(name);
		if (!added.has(method_name)) {
			r_list->push_back(get_method_info(method_name));
			added.insert(method_name);
		}
	}
	static const char *callbacks[] = { "_enter_tree", "_ready", "_process", "_physics_process", "_input", "_unhandled_input", "_exit_tree", "_notification" };
	for (const char *callback : callbacks) {
		const StringName method_name(callback);
		if (!added.has(method_name)) {
			r_list->push_back(get_method_info(method_name));
		}
	}
}

bool LuaScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	Array exports = _lua_export_declarations(source_code);
	for (int i = 0; i < exports.size(); i++) {
		Dictionary item = exports[i];
		if (StringName(item["name"]) == p_property) {
			r_value = item["default"];
			return true;
		}
	}
	return false;
}

void LuaScript::get_script_property_list(List<PropertyInfo> *r_list) const {
	Array exports = _lua_export_declarations(source_code);
	for (int i = 0; i < exports.size(); i++) {
		Dictionary item = exports[i];
		PropertyInfo property((Variant::Type)(int)item["type"], StringName(item["name"]));
		property.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE | PROPERTY_USAGE_EDITOR;
		r_list->push_back(property);
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
	if (loaded) {
		Array exports = _lua_export_declarations(script->get_source_code());
		for (int i = 0; i < exports.size(); i++) {
			Dictionary item = exports[i];
			String name = item["name"];
			if (runtime->get_global_value(name).get_type() == Variant::NIL) {
				runtime->set_global_value(name, item["default"]);
			}
		}
	}
	if (!loaded) {
		ERR_PRINT(vformat("Could not execute attached Lua script: %s", String(result.get("error", "unknown error"))));
	}
}

bool LuaScriptInstance::set(const StringName &p_name, const Variant &p_value) {
	if (!loaded || runtime.is_null() || script.is_null()) return false;
	Array exports = _lua_export_declarations(script->get_source_code());
	for (int i = 0; i < exports.size(); i++) {
		Dictionary item = exports[i];
		if (StringName(item["name"]) == p_name) {
			runtime->set_global_value(String(p_name), p_value);
			return true;
		}
	}
	return false;
}

bool LuaScriptInstance::get(const StringName &p_name, Variant &r_ret) const {
	if (!loaded || runtime.is_null() || script.is_null()) return false;
	Array exports = _lua_export_declarations(script->get_source_code());
	for (int i = 0; i < exports.size(); i++) {
		Dictionary item = exports[i];
		if (StringName(item["name"]) == p_name) {
			r_ret = runtime->get_global_value(String(p_name));
			if (r_ret.get_type() == Variant::NIL) r_ret = item["default"];
			return true;
		}
	}
	return false;
}

void LuaScriptInstance::get_property_list(List<PropertyInfo> *r_properties) const {
	if (script.is_valid()) script->get_script_property_list(r_properties);
}

Variant::Type LuaScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (script.is_valid()) {
		Array exports = _lua_export_declarations(script->get_source_code());
		for (int i = 0; i < exports.size(); i++) {
			Dictionary item = exports[i];
			if (StringName(item["name"]) == p_name) {
				if (r_is_valid) *r_is_valid = true;
				return (Variant::Type)(int)item["type"];
			}
		}
	}
	if (r_is_valid) *r_is_valid = false;
	return Variant::NIL;
}

void LuaScriptInstance::get_method_list(List<MethodInfo> *r_list) const {
	if (!script.is_null()) script->get_script_method_list(r_list);
}

bool LuaScriptInstance::has_method(const StringName &p_method) const {
	return !script.is_null() && script->has_method(p_method);
}

int LuaScriptInstance::get_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (script.is_null() || !script->has_method(p_method)) {
		if (r_is_valid) *r_is_valid = false;
		return 0;
	}
	if (r_is_valid) *r_is_valid = true;
	return script->get_method_info(p_method).arguments.size();
}

Variant LuaScriptInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	if (!loaded || runtime.is_null()) {
		r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}
	Array args;
	for (int i = 0; i < p_argcount; i++) {
		args.push_back(p_args[i] ? *p_args[i] : Variant());
	}
	Dictionary result = runtime->call_function_args(String(p_method), args);
	if (!bool(result.get("ok", false))) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}
	return result.get("value", Variant());
}

void LuaScriptInstance::notification(int p_notification, bool p_reversed) {
	(void)p_reversed;
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
	} else if (script.is_valid() && script->has_method("_notification")) {
		Array args;
		args.push_back(p_notification);
		runtime->call_function_args("_notification", args);
	}
}

ScriptLanguage *LuaScriptInstance::get_language() {
	return LuaScriptLanguage::get_singleton();
}
