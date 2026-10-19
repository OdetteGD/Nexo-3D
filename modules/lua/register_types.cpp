/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "lua_runtime.h"
#include "lua_script.h"
#include "lua_script_runner.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"

static Ref<ResourceFormatLoader> lua_resource_loader;
static Ref<ResourceFormatSaver> lua_resource_saver;

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/lua_syntax_highlighter.h"
#include "editor/script/script_editor_plugin.h"

static void _lua_editor_init() {
	if (ScriptEditor::get_singleton()) {
		Ref<LuaSyntaxHighlighter> highlighter;
		highlighter.instantiate();
		ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);
	}
}
#endif

class LuaResourceLoader : public ResourceFormatLoader {
public:
	Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override {
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			if (r_error) {
				*r_error = ERR_CANT_OPEN;
			}
			return Ref<Resource>();
		}
		Ref<LuaScript> script;
		script.instantiate();
		script->set_path(p_path);
		script->set_source_code(file->get_as_text());
		Error err = script->reload();
		if (r_error) {
			*r_error = err;
		}
		if (err != OK) {
			return Ref<Resource>();
		}
		return script;
	}
	void get_recognized_extensions(List<String> *p_extensions) const override { p_extensions->push_back("lua"); }
	bool handles_type(const String &p_type) const override { return p_type == "LuaScript" || p_type == "Script"; }
	String get_resource_type(const String &p_path) const override { return p_path.get_extension().to_lower() == "lua" ? "LuaScript" : String(); }
	void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false) override {}
	void get_classes_used(const String &p_path, HashSet<StringName> *r_classes) override {}
};

class LuaResourceSaver : public ResourceFormatSaver {
public:
	Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override {
		Ref<LuaScript> script = p_resource;
		if (script.is_null()) {
			return ERR_INVALID_PARAMETER;
		}
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
		if (file.is_null()) {
			return ERR_CANT_CREATE;
		}
		file->store_string(script->get_source_code());
		return file->get_error();
	}
	void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override {
		if (Object::cast_to<LuaScript>(*p_resource)) {
			p_extensions->push_back("lua");
		}
	}
	bool recognize(const Ref<Resource> &p_resource) const override { return Object::cast_to<LuaScript>(*p_resource) != nullptr; }
};

void initialize_lua_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(LuaScript);
		GDREGISTER_CLASS(LuaRuntime);
		GDREGISTER_CLASS(LuaScriptRunner);
		LuaScriptLanguage *language = memnew(LuaScriptLanguage);
		ScriptServer::register_language(language);

		lua_resource_loader = Ref<ResourceFormatLoader>(memnew(LuaResourceLoader));
		lua_resource_saver = Ref<ResourceFormatSaver>(memnew(LuaResourceSaver));
		ResourceLoader::add_resource_format_loader(lua_resource_loader);
		ResourceSaver::add_resource_format_saver(lua_resource_saver);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		EditorNode::add_init_callback(_lua_editor_init);
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(LuaSyntaxHighlighter);
	}
#endif
}

void uninitialize_lua_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ScriptServer::unregister_language(LuaScriptLanguage::get_singleton());
		ResourceLoader::remove_resource_format_loader(lua_resource_loader);
		ResourceSaver::remove_resource_format_saver(lua_resource_saver);
		lua_resource_loader.unref();
		lua_resource_saver.unref();
		memdelete(LuaScriptLanguage::get_singleton());
	}
}
