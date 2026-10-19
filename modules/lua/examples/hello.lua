-- Nexo-3D Lua example
-- This is standard Lua; engine bindings will be provided by the runtime adapter.

local function greet(name)
    assert(type(name) == "string", "name must be a string")
    return "Hello, " .. name .. "!"
end

print(greet("Nexo-3D"))

-- Future Nexo API example (not available until the engine binding is implemented):
-- function _ready()
--     Nexo.log("Lua script ready")
-- end
