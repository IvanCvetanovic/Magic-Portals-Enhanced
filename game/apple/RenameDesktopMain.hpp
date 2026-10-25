#pragma once

// FORCE-INCLUDED INTO game/main.cpp ALONE, on an Apple target (game/CMakeLists.txt, planning step 206, PORT-8): it takes the name `main` away from the one function
// main.cpp defines it for, so that the program's real `main` can be the Apple entry's.
//
// WHY. main.cpp ends with `int main(...) { return MagicPortalsMain(...); }` for every platform but Android, whose engine owns the process's entry. A macOS program's
// main() is game/macos/MacMain.mm's (it finds the data in the bundle and puts the shaders where the engine may write), and an iOS program's is the engine's own, in
// SupersonicCore (engine/src/platform/ios/IOSApp.mm): a second `_main` is a duplicate symbol in both links. main.cpp is a source of the Windows program and is not edited
// by this step, so the definition is renamed here instead, for the Apple targets only. The one-line source guard that would replace this file
// (`#if !defined(__ANDROID__) && !defined(__APPLE__)` round main.cpp's last two lines) waits for a step that may relink the Windows program (docs/apple.md).
//
// A FUNCTION-LIKE macro, and not `-Dmain=name`: main.cpp reads a header (MagicPortalsLayer.hpp) that has a member called `main` (`MainMenu::Button main`), which an
// object-like macro would rename in this one translation unit and in no other. `main` is replaced only where a parenthesis follows it, which main.cpp's definition has and
// that member does not (the check is a preprocessor run that counts the replacements: docs/apple.md; CMake itself drops a function-like -D, which is why this is a file).
#define main(...) MagicPortalsDesktopMainUnusedOnApple(__VA_ARGS__)
