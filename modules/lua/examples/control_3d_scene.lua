-- Bind this runtime to a Node3D with runtime.set_target_node(node).
Nexo.log("Lua 3D scene controller started")

-- Example hierarchy: Player/Model
-- Nexo.node.select("Model")
-- Nexo.node.set_position(0, 1, 0)
-- Nexo.node.rotate_y(0.25)
-- Nexo.node.set_visible(true)
--
-- Add a PackedScene as a child of the selected node:
-- Nexo.scene.add_child("res://scenes/effect.tscn")
--
-- Change the active scene:
-- Nexo.scene.change("res://scenes/level_02.tscn")

local x, y, z = Nexo.node.get_position()
Nexo.log("Target position:", x, y, z)
