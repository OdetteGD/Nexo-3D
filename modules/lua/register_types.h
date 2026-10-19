#pragma once

#include "modules/register_module_types.h"

void initialize_lua_module(ModuleInitializationLevel p_level);
void uninitialize_lua_module(ModuleInitializationLevel p_level);
