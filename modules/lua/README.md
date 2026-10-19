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
- Native `ScriptInstance` method discovery for user-defined top-level Lua `function name(...)` declarations, including method argument metadata exposed to Godot.
- Callback argument forwarding from Godot to Lua for nil, booleans, integers, floats, strings, `Vector2`, and `Vector3` values.

## Pure Lua scene setup

1. Add a **LuaScriptRunner** node as a child of the node you want to control.
2. Set its **Script Path** property to a project `.lua` file, such as `res://scripts/player.lua`.
3. Define optional global `function _ready()` and `function _process(delta)` callbacks in Lua. The runner executes the file, then dispatches these callbacks from native C++—no GDScript wrapper is needed.
4. The runner targets its parent node when one exists. `Nexo.node.rotate_y(delta)`, `set_position(x, y, z)`, `translate(x, y, z)`, and `set_visible(bool)` operate on a selected Node3D. Use `Nexo.node.select("Child/Path")` to select a child relative to the current target.

`LuaScriptRunner` and Lua 5.4 are compiled as part of the engine module, including Android export templates when `module_lua_enabled=yes`. See `examples/main.lua` for a pure Lua lifecycle example and `examples/control_3d_scene.lua` for scene operations.

## Remaining integration work

`.lua` is registered as a native `Script` resource and is intended to be attachable through Godot’s script workflow. This implementation is still not feature-parity with GDScript: exported/typed properties, script-defined signals, inheritance, complete editor semantic completion, parser diagnostics in the Script Editor Problems panel, breakpoints and step-through debugging, source-mapped stack frames, full Lua-to-Variant return-value conversion, and broad Node/Object/Input APIs need additional implementation and validation. Callback argument conversion currently supports nil, booleans, integers, floats, strings, `Vector2`, and `Vector3`; other Variant types are passed as `nil` until safe conversions are implemented. The highlighter is lexical, while parser errors are returned by `check_syntax`. The instruction budget limits Lua VM instructions, not time spent in native engine calls. Treat scripts as trusted project code and expose only reviewed bindings.

## Compatibility

- Lua 5.4.8: bundled runtime.
- LuaJIT 2.1: separate optional backend with Lua 5.1 semantics.
- Roblox Luau: separate language/runtime.
- LÖVE callbacks/modules: not included by this bridge.


## Native `.lua` Script resource

The Lua module registers `LuaScriptLanguage` with `ScriptServer` and installs a `.lua` resource loader/saver. In a tools-enabled editor build, Lua appears alongside GDScript in the language selector for creating a script. Existing `.lua` files load as `LuaScript` resources and can be attached to Node-derived objects.

The initial native lifecycle bridge supports these global Lua callbacks:

- `function _enter_tree()`
- `function _ready()`
- `function _process(delta)`
- `function _physics_process(delta)`
- `function _exit_tree()`

Example:

```lua
function _ready()
    Nexo.log("Attached Lua script is running")
end

function _process(delta)
    Nexo.node.rotate_y(delta * 0.5)
end
```

The embedded runtime intentionally exposes a restricted standard library and the `Nexo` API. Script inheritance, exported properties, custom signals, complete debugger integration, and editor semantic completion are not implemented yet. The Android workflow builds release engine templates for arm64, arm32, and x86_64; these are engine templates, not a ready-made APK.
