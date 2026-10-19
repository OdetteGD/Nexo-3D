Lua scripting in Nexo-3D
=========================

Nexo-3D is preparing a portable Lua scripting layer for gameplay logic and editor workflows.

Runtime compatibility
---------------------

The initial CI reference is Lua 5.4. LuaJIT 2.1 is a candidate backend for platforms where its Lua 5.1-compatible semantics and native build support fit the target. Scripts should avoid relying on version-specific behavior unless a project explicitly selects that runtime.

Roblox Luau is a separate language/runtime, not the same thing as stock Lua or LuaJIT. Supporting Luau would require a separate adapter and compiler integration. Roblox services such as `game`, `Instance`, and `workspace` are not automatically available in Nexo-3D.

Editor diagnostics
------------------

Syntax errors must come from the selected Lua compiler/parser, with filename and line information preserved. A highlighter alone cannot reliably detect syntax errors. Runtime exceptions should be reported separately from parse/compile diagnostics.

Security and performance
------------------------

Scripts should execute in a restricted environment with explicit engine bindings. Avoid exposing unrestricted filesystem, process, package, or native-library access to project scripts. Use protected calls and enforce execution budgets. Do not execute user scripts on the render thread.

Implementation status
---------------------

The Lua documentation, examples, compiler-validation fixtures, and CI workflow establish a starting point. The engine VM, Android packaging, editor syntax highlighter, script resource type, and engine API bindings still require implementation and platform testing.
