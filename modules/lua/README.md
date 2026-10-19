# Nexo-3D Lua scripting module

Nexo-3D now vendors the **Lua 5.4.8** runtime under `thirdparty/lua` (MIT license) and builds it through `modules/lua/SCsub`. The module exposes a small engine-facing `LuaRuntime` object. LuaJIT and Roblox Luau are different runtimes/language targets and are not silently treated as interchangeable with Lua 5.4.

## Current implementation

- Embedded Lua 5.4.8 C runtime, compiled with the engine module for desktop and mobile targets.
- `LuaRuntime.check_syntax(source, source_name)` compiles source without running it and returns a Dictionary with `ok`, `error`, `line`, and `kind`.
- `LuaRuntime.execute_string(source, source_name)` executes a chunk with protected calls and the same structured result.
- `LuaRuntime.check_file(path)` and `LuaRuntime.execute_file(path)` read and validate/execute a `.lua` file through Godot's FileAccess abstraction.
- A restricted standard-library setup: base, table, string, math, and utf8 are opened; `io`, `os`, `debug`, and `package` are not opened. Dynamic/file loading helpers and garbage-collector control are removed from the base environment.
- Configurable instruction budget with a Lua VM count hook to interrupt runaway scripts.
- Editor syntax highlighter for Lua keywords, strings, comments, numbers, built-ins, and function calls.
- CI compiles the vendored C sources and smoke-tests valid execution and invalid syntax diagnostics.

## Quick use from engine-side GDScript

```gdscript
var lua := LuaRuntime.new()
var result: Dictionary = lua.check_file("res://scripts/hello.lua")
if not result.ok:
    push_error("%s:%s: %s" % [
        "res://scripts/hello.lua",
        result.line,
        result.error
    ])
else:
    var run_result: Dictionary = lua.execute_file("res://scripts/hello.lua")
    if not run_result.ok:
        push_error("%s: %s" % [run_result.kind, run_result.error])
```

Or execute a string directly:

```gdscript
var lua := LuaRuntime.new()
lua.set_instruction_budget(1000000)
var result := lua.execute_string("local n = 6 * 7; assert(n == 42)", "res://inline.lua")
assert(result.ok, result.error)
```

## Security and current limitations

This is an embedded runtime API and editor syntax-highlighting layer, not yet a complete `ScriptLanguage` implementation. `.lua` files can be read and run explicitly through `LuaRuntime`, but are not yet first-class attachable scene scripts with automatic lifecycle callbacks. The editor highlighter is lexical only; compile diagnostics come from the actual Lua parser through `check_syntax`. The instruction budget limits Lua VM instructions, not native engine calls or wall-clock time. Run untrusted scripts in a separately controlled environment and expose only explicitly approved engine objects/functions.

## Runtime compatibility

- **Lua 5.4.8**: bundled default.
- **LuaJIT 2.1**: a possible separate backend; LuaJIT follows Lua 5.1 semantics and requires a distinct compatibility test matrix.
- **Roblox Luau**: separate language/runtime, with syntax and API differences; not provided by this Lua 5.4 runtime.
- **LÖVE**: its callbacks and modules are not assumed to exist; a LÖVE compatibility layer would be a separate implementation.

## Test

The GitHub Actions workflow runs the syntax fixtures and compiles the vendored runtime with a C smoke test. Engine-level Android/editor builds still need to be run to validate the complete Nexo-3D module on each target ABI.
