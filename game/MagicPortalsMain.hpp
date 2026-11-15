#pragma once

#include <string>

// THE GAME'S ENTRY, as a function (planning step 203, PORT-5). main.cpp holds the whole of the game; a desktop's main() calls it, and so does
// Android's SupersonicMain (game/android/AndroidMain.cpp), which first unpacks the APK's data and builds the argument list a command line would
// have carried. One definition, so the two cannot drift: nothing about a desktop run changed when this was split out of main().
//
// argv[0] and the flags, as main() receives them. Returns the process's exit code: 0 for a run that ended clean, 1 for any failure (the reason is
// in the log and on stderr, and in a message box for a player's run). Never throws.
int MagicPortalsMain(int argc, char** argv);

// The system's language, for a shell that can ask the system better than a desktop's environment can (planning step 245, LANG-3). Android's activity carries
// no LANG, and a Mac app opened from the Finder and an iPhone's carry none either, so their entries read it from the platform and hand it over here BEFORE
// MagicPortalsMain, as the locale's own text (`pt`, `pt-BR`): sim/Language.hpp's CodeForLocale says which texts name one of the eleven languages. It
// is NOT a command-line flag: `--lang` forces a language and would hide every saved choice. What MagicPortalsMain does with it - a first run's language
// when there is no language.json and no --lang, for a player's run only - is Language::ApplySystemLocale's. A shell that never calls it, or calls it with
// an empty text, leaves the game to ask the system itself (Windows reads its UI language; Linux reads LC_ALL, LC_MESSAGES and LANG). Call it from the
// game's own thread, before MagicPortalsMain.
void MagicPortalsSetSystemLocale(const std::string& locale);
