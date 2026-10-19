def can_build(env, platform):
    # Lua 5.4.8 is vendored under thirdparty/lua and has no external dependencies.
    return True


def configure(env):
    pass


def get_doc_classes():
    return ["LuaRuntime", "LuaScript", "LuaScriptRunner"]


def get_doc_path():
    return "doc_classes"
