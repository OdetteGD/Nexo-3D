/**************************************************************************/
/*  lua_script.h                                                          */
/**************************************************************************/
#pragma once

#include "core/object/script_language.h"
#include "lua_runtime.h"

#ifdef TOOLS_ENABLED
#include "core/object/editor_language.h"

class LuaEditorLanguage : public EditorLanguage {};
#endif

class LuaScriptLanguage;

class LuaScript : public Script {
	GDCLASS(LuaScript, Script);

	String source_code;
	bool valid = false;

protected:
	static void _bind_methods();

public:
	bool can_instantiate() const override { return valid; }
	Ref<Script> get_base_script() const override { return Ref<Script>(); }
	StringName get_global_name() const override { return StringName(); }
	bool inherits_script(const Ref<Script> &p_script) const override { return p_script.ptr() == this; }
	StringName get_instance_base_type() const override { return "Node"; }
	ScriptInstance *instance_create(Object *p_this) override;
	bool has_source_code() const override { return true; }
	String get_source_code() const override { return source_code; }
	void set_source_code(const String &p_code) override;
	Error reload(bool p_keep_state = false) override;
	bool has_method(const StringName &p_method) const override;
	bool has_static_method(const StringName &p_method) const override { return false; }
	MethodInfo get_method_info(const StringName &p_method) const override;
	bool is_tool() const override { return false; }
	bool is_script_valid() const override { return valid; }
	bool is_abstract() const override { return false; }
	ScriptLanguage *get_language() const override;
	bool has_script_signal(const StringName &p_signal) const override;
	void get_script_signal_list(List<MethodInfo> *r_signals) const override;
	bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;
	void get_script_method_list(List<MethodInfo> *r_list) const override;
	void get_script_property_list(List<PropertyInfo> *r_list) const override;
	const Variant get_rpc_config() const override { return Dictionary(); }

#ifdef TOOLS_ENABLED
	StringName get_doc_class_name() const override { return StringName(); }
	Vector<DocData::ClassDoc> get_documentation() const override { return Vector<DocData::ClassDoc>(); }
	String get_class_icon_path() const override { return String(); }
#endif

	LuaScript();
};

class LuaScriptLanguage : public ScriptLanguage {
	static LuaScriptLanguage *singleton;
#ifdef TOOLS_ENABLED
	LuaEditorLanguage editor_language;
#endif

public:
	static LuaScriptLanguage *get_singleton() { return singleton; }
	String get_name() const override { return "Lua"; }
	void init() override {}
	String get_type() const override { return "Lua"; }
	String get_extension() const override { return "lua"; }
	void finish() override {}
#ifdef TOOLS_ENABLED
	EditorLanguage *get_editor_language() override { return &editor_language; }
#endif
	Vector<String> get_reserved_words() const override;
	bool is_control_flow_keyword(const String &p_string) const override;
	Vector<String> get_comment_delimiters() const override;
	Vector<String> get_doc_comment_delimiters() const override;
	Vector<String> get_string_delimiters() const override;
	Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	bool supports_builtin_mode() const override { return false; }
	String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const override;
	void add_global_constant(const StringName &p_variable, const Variant &p_value) override {}
	String debug_get_error() const override { return String(); }
	int debug_get_stack_level_count() const override { return 0; }
	int debug_get_stack_level_line(int p_level) const override { return -1; }
	String debug_get_stack_level_function(int p_level) const override { return String(); }
	String debug_get_stack_level_source(int p_level) const override { return String(); }
	void debug_get_stack_level_locals(int p_level, List<String> *r_locals, List<Variant> *r_values, int p_max_subitems = -1, int p_max_depth = -1) override {}
	void debug_get_stack_level_members(int p_level, List<String> *r_members, List<Variant> *r_values, int p_max_subitems = -1, int p_max_depth = -1) override {}
	void debug_get_globals(List<String> *r_globals, List<Variant> *r_values, int p_max_subitems = -1, int p_max_depth = -1) override {}
	String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems = -1, int p_max_depth = -1) override { return String(); }
	void reload_all_scripts() override {}
	void reload_scripts(const Array &p_scripts) override {}
	void reload_tool_script(const Ref<Script> &p_script) override {}
	void get_public_functions(List<MethodInfo> *r_functions) const override {}
	void get_public_constants(List<Pair<String, Variant>> *r_constants) const override {}
	void get_public_annotations(List<MethodInfo> *r_annotations) const override {}
	void profiling_start() override {}
	void profiling_stop() override {}
	void profiling_set_save_native_calls(bool p_enable) override {}
	int profiling_get_accumulated_data(ProfilingInfo *r_info_arr, int p_info_max) override { return 0; }
	int profiling_get_frame_data(ProfilingInfo *r_info_arr, int p_info_max) override { return 0; }

	LuaScriptLanguage();
	~LuaScriptLanguage();
};

class LuaScriptInstance : public ScriptInstance {
	Object *owner = nullptr;
	Ref<LuaScript> script;
	Ref<LuaRuntime> runtime;
	bool loaded = false;

public:
	bool set(const StringName &p_name, const Variant &p_value) override;
	bool get(const StringName &p_name, Variant &r_ret) const override;
	void get_property_list(List<PropertyInfo> *r_properties) const override;
	Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;
	void validate_property(PropertyInfo &p_property) const override {}
	bool property_can_revert(const StringName &p_name) const override { return false; }
	bool property_get_revert(const StringName &p_name, Variant &r_ret) const override { return false; }
	void get_method_list(List<MethodInfo> *r_list) const override;
	bool has_method(const StringName &p_method) const override;
	int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override;
	Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;
	void notification(int p_notification, bool p_reversed = false) override;
	Script *get_script() const override { return script.ptr(); }
	ScriptLanguage *get_language() override;
	Object *get_owner() override { return owner; }
	const Variant get_rpc_config() const override { return Dictionary(); }

	LuaScriptInstance(LuaScript *p_script, Object *p_owner);
	~LuaScriptInstance() override = default;
};
