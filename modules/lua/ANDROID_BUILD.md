# Lua scripting and Android builds

The `lua-android-ci.yml` workflow checks out submodules, builds the Linux editor to compile the Lua module, and builds the Android arm64 release template. The Android template is uploaded as a GitHub Actions artifact for 14 days.

## Run it

1. Push to `feature/lua-scripting-foundation`, open a pull request targeting that branch, or start **Lua scripting and Android build** from Actions with **Run workflow**.
2. Inspect both jobs and resolve compiler or Android SDK errors before relying on the artifact.
3. Download `nexo-3d-android-arm64-template` from a successful workflow run.

## APK export is a separate step

The Android job builds an engine export template, not a game APK. An APK additionally needs a Godot project, export preset, Android packaging setup, and (for release distribution) signing configuration. Do not store signing keys or passwords in the repository; configure them as GitHub Actions secrets and use a protected release workflow.

## Lua integration status

This CI workflow verifies that the Lua module is present and compiles it as part of the engine. A compiled Lua runtime, syntax highlighter, or runner node alone does **not** prove that `.lua` is a native Godot `Script` resource. First-class editor integration requires a registered `ScriptLanguage` implementation and a `Script` resource implementation with parsing, validation, instance creation, editor integration, and appropriate lifecycle/error handling. Verify those interfaces against this branch's exact Godot version before marking native ScriptLanguage support complete.

## Validation limits

The workflow is a compile/build gate. It does not claim runtime validation on a physical Android device, APK installation, or successful execution of a Lua-driven scene. Add device/emulator smoke tests and an exported sample project before treating those behaviors as verified.


## Native Lua scripts

The module now registers `LuaScriptLanguage` with `ScriptServer`, plus a `.lua` resource loader and saver. This enables a tools-enabled editor to offer Lua alongside GDScript when creating scripts, and lets `.lua` files load as `LuaScript` resources. The initial runtime bridge supports Node lifecycle callbacks and the restricted `Nexo` API.

The Android workflow currently checks for the new ScriptLanguage/resource integration sources and compiles Android release templates only. A green Android build is required before treating this change as build-verified; the workflow artifact is an engine template, not an APK. Script inheritance, exported Lua properties, custom signals, and full debugger/editor semantic support remain follow-up work.
