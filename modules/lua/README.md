# Nexo-3D Lua scripting foundation

This directory defines the first integration layer for Lua scripting in Nexo-3D (the Godot-derived engine source).

## Runtime decision

Use **Lua 5.4 as the initial portable reference runtime** and evaluate **LuaJIT 2.1** as an optional performance backend. LuaJIT implements Lua 5.1 language semantics with selected extensions; it is not a drop-in Lua 5.4 runtime. Keep the engine-facing API independent of either runtime so Android builds can select a supported backend.

This commit is a foundation and design contract, not a claim that the VM is already embedded in the engine executable.

## Planned engine integration

1. Add a dedicated engine module with a small adapter interface for create/destroy state, load source, execute protected calls, and format errors.
2. Expose only an allowlisted Nexo API to scripts. Do not open filesystem, process, package-loading, or native-library APIs by default.
3. Use protected calls and a per-script instruction/time budget. Never run scripts on the rendering thread.
4. Map script resources to `.lua` files and provide reload, lifecycle hooks, and debugger-friendly source names.
5. Keep runtime choice behind build flags so Android ABI builds can ship LuaJIT where supported and fall back to Lua 5.4.

## Editor requirements

- Lua syntax highlighting for keywords, strings, numbers, comments, operators, and built-in functions.
- Parse/compile diagnostics with file path, line and column where the runtime provides them.
- A Problems panel with clickable diagnostics; syntax errors must not be presented as runtime errors.
- Validate on save and before play/build, with cancellation/debouncing for editor responsiveness.
- Preserve source text and report errors instead of crashing the editor.

Syntax highlighting is presentation only; use the real Lua parser/compiler for syntax validation.

## Example

See `examples/hello.lua` for a runtime-neutral sample. The example uses ordinary Lua and does not assume Roblox's proprietary APIs or LÖVE's global callbacks exist in Nexo-3D.
