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
- Lua callback return conversion back into Godot `Variant` values for nil, booleans, integers, floats, strings, `Vector2`, `Vector3`, Lua sequence tables (`Array`), and string/integer-keyed Lua tables (`Dictionary`). Nested table conversion is depth-limited.
- Inspector properties declared with comment annotations such as `-- @export speed: float = 2.5`; supported types are `int`, `float`/`number`, `bool`/`boolean`, `string`, `Vector2`, and `Vector3`.
- Custom script signal metadata declared with `-- @signal health_changed(amount: int)` and guarded emission through `Nexo.signal.emit("health_changed", amount)`; emission checks that the signal exists on the target Node.
- Script inheritance through `-- @extends res://base.lua`; the base source executes first in the same Lua state, then derived definitions override same-named globals.
- Object arguments, including InputEvents, are exposed only as snapshots of allowlisted properties (class name, pressed/key/button fields and positions), not as raw pointers or unrestricted Object userdata.

## Pure Lua scene setup

1. Add a **LuaScriptRunner** node as a child of the node you want to control.
2. Set its **Script Path** property to a project `.lua` file, such as `res://scripts/player.lua`.
3. Define optional global `function _ready()` and `function _process(delta)` callbacks in Lua. The runner executes the file, then dispatches these callbacks from native C++—no GDScript wrapper is needed.
4. The runner targets its parent node when one exists. `Nexo.node.rotate_y(delta)`, `set_position(x, y, z)`, `translate(x, y, z)`, and `set_visible(bool)` operate on a selected Node3D. Use `Nexo.node.select("Child/Path")` to select a child relative to the current target.

`LuaScriptRunner` and Lua 5.4 are compiled as part of the engine module, including Android export templates when `module_lua_enabled=yes`. See `examples/main.lua` for a pure Lua lifecycle example and `examples/control_3d_scene.lua` for scene operations.

## Remaining integration work

`.lua` is registered as a native `Script` resource and is intended to be attachable through Godot’s script workflow. This implementation is still not feature-parity with GDScript: complete editor semantic completion, parser diagnostics in the Script Editor Problems panel, actual breakpoints and step-through debugging, source-mapped stack frames, broader Node/Object/Input APIs, and full parity with Godot's debugger need additional implementation and validation. Exported properties and custom signals are currently annotation-based and require editor/runtime validation. Callback argument conversion currently supports nil, booleans, integers, floats, strings, `Vector2`, and `Vector3`; other Variant types are passed as `nil` until safe conversions are implemented. The highlighter is lexical, while parser errors are returned by `check_syntax`. The instruction budget limits Lua VM instructions, not time spent in native engine calls. Treat scripts as trusted project code and expose only reviewed bindings.

## Compatibility

- Lua 5.4.8: bundled runtime.
- LuaJIT 2.1: separate optional backend with Lua 5.1 semantics.
- Roblox Luau: separate language/runtime.
- LÖVE callbacks/modules: not included by this bridge.


## Native `.lua` Script resource

The Lua module registers `LuaScriptLanguage` with `ScriptServer` and installs a `.lua` resource loader/saver. In a tools-enabled editor build, Lua appears alongside GDScript in the language selector for creating a script. Existing `.lua` files load as `LuaScript` resources and can be attached to Node-derived objects.

For Inspector properties, add one annotation per property comment; for example:

```lua
-- @export speed: float = 2.5
-- @export health: int = 100
-- @export is_invulnerable: bool = false
-- @export title: string = "Player"

function _ready()
    Nexo.log("speed", speed, "health", health)
end
```

The annotation creates an Inspector property backed by the matching Lua global. This is a deliberately small first format; it is not a Lua type declaration and only the listed types are supported.



Inheritance can be declared in a derived script:

```lua
-- @extends res://scripts/base.lua

function _ready()
    Nexo.log("Derived ready; base globals are available")
end
```

The base script runs before the derived source in the same runtime, so derived top-level function definitions replace base functions of the same name. This initial form does not yet merge inherited Inspector property metadata or provide native debugger stepping.

Custom signals use this annotation format:

```lua
-- @signal health_changed(amount: int)

function take_damage(amount)
    health = health - amount
    Nexo.signal.emit("health_changed", health)
end
```

The signal declaration is surfaced through Godot's script signal metadata. `Nexo.signal.emit` only emits signals declared on the target Node; it does not expose arbitrary Object pointers to Lua. Signal argument annotations currently support `int`, `float`/`number`, `bool`, `string`, `Vector2`, and `Vector3`.

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
