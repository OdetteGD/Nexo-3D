-- Pure Lua gameplay script. Add a LuaScriptRunner child to your scene and set
-- Script Path to res://modules/lua/examples/main.lua (or copy it to your project).
-- No GDScript wrapper is required.

function _ready()
    Nexo.log("Pure Lua script started!")
end

function _process(delta)
    -- The runner targets its parent node. If that parent is a Node3D, rotate it.
    Nexo.node.rotate_y(delta * 0.5)
end
