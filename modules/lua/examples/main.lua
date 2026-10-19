-- Pure Lua script attached through a LuaScriptRunner node.
-- Add a LuaScriptRunner node to your scene and set Script Path to this file.
-- This script does not require GDScript.

function _ready()
    Nexo.log("Pure Lua script started!")
    -- The runner itself is a Node. Select its parent if it is a Node3D.
    if Nexo.node.select("..") then
        Nexo.node.rotate_y(0.0)
    end
end

function _process(delta)
    -- Keep gameplay code in Lua; delta is seconds since the previous frame.
    if Nexo.node.select("..") then
        Nexo.node.rotate_y(delta * 0.5)
    end
end
