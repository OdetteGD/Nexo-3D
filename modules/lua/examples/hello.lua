-- Nexo-3D Lua example
-- This is standard Lua. Nexo.log is provided by LuaRuntime.

local function greet(name)
    assert(type(name) == "string", "name must be a string")
    return "Hello, " .. name .. "!"
end

Nexo.log(greet("Nexo-3D"))

-- Future Nexo API example (not available until the engine binding is implemented):
-- function _ready()
--     Nexo.log("Lua script ready")
-- end
