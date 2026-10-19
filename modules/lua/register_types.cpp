#include "register_types.h"

#include "lua_runtime.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/lua_syntax_highlighter.h"

static void _lua_editor_init() {
    if (ScriptEditor::get_singleton()) {
        Ref<LuaSyntaxHighlighter> highlighter;
        highlighter.instantiate();
        ScriptEditor::get_singleton()->register_syntax_highlighter(highlighter);
    }
}
#endif

void initialize_lua_module(ModuleInitializationLevel p_level) {
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(LuaRuntime);
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
    (void)p_level;
}
