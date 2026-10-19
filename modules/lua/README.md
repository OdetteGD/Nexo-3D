# Nexo-3D Lua scripting module

Nexo-3D vendors **Lua 5.4.8** under `thirdparty/lua` (MIT license), built through `modules/lua/SCsub`. LuaJIT and Roblox Luau are separate runtimes and are not treated as interchangeable with Lua 5.4.

## Implemented runtime API

- `LuaRuntime.check_syntax(source, source_name)`: compile source without executing it; returns `ok`, `error`, `line`, and `kind`.
- `LuaRuntime.check_file(path)` / `execute_file(path)`: check or execute a `.lua` file through FileAccess.
- `LuaRuntime.execute_string(source, source_name)`: protected execution.
- Configurable Lua VM instruction budget.
- Restricted libraries: no `io`, `os`, `debug`, or `package`; removes base `dofile`, `loadfile`, `load`, and `collectgarbage`.
- `Nexo.log(...)`: engine log output.
- `LuaRuntime.set_target_node(node)`: selects the initial Node context for engine bindings.
- `Nexo.node.select("Child/Path")`: select a child node relative to the current target.
- `Nexo.node.set_position(x, y, z)`, `translate(x, y, z)`, `rotate_y(radians)`, `set_visible(bool)`, and `get_position()`: control the selected Node3D.
- `Nexo.scene.add_child("res://scene.tscn")`: instantiate a PackedScene as a child of the selected node.
- `Nexo.scene.change("res://scene.tscn")`: request a change to the active SceneTree scene.
- Lua editor syntax highlighter for keywords, strings, comments, numbers, built-ins, and function calls.

## Example setup

Create a runtime and bind it to a Node3D from GDScript:

```gdscript
var lua := LuaRuntime.new()
lua.set_target_node(self)
var check := lua.check_file("res://scripts/control_3d_scene.lua")
if not check.ok:
    push_error("%s:%s: %s" % ["res://scripts/control_3d_scene.lua", check.line, check.error])
else:
    var result := lua.execute_file("res://scripts/control_3d_scene.lua")
    if not result.ok:
        push_error("%s: %s" % [result.kind, result.error])
```

See `examples/control_3d_scene.lua` for the supported node and scene bridge.

## Limitations still remaining

This is not yet a full first-class Godot `ScriptLanguage` implementation. Creating/opening arbitrary `.lua` files as native attachable Script resources, automatic `_ready()` / `_process(delta)` lifecycle dispatch, live parser diagnostics in the Script Editor Problems panel, breakpoints/step-through debugging, source-mapped stack frames, and full SceneTree/Node/Object APIs still require dedicated integration work. The highlighter is lexical, while parser errors are returned by `check_syntax`. The instruction budget limits Lua VM instructions, not time spent in native engine calls. Treat scripts as trusted project code and expose only reviewed bindings.

## Compatibility

- Lua 5.4.8: bundled runtime.
- LuaJIT 2.1: separate optional backend with Lua 5.1 semantics.
- Roblox Luau: separate language/runtime.
- LÖVE callbacks/modules: not included by this bridge.
