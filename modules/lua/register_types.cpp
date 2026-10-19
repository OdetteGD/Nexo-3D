#include "register_types.h"

#include "lua_runtime.h"

#include "core/object/class_db.h"

void initialize_lua_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(LuaRuntime);
}

void uninitialize_lua_module(ModuleInitializationLevel p_level) {
    (void)p_level;
}
