#pragma once

// THE GAME'S ENTRY, as a function (planning step 203, PORT-5). main.cpp holds the whole of the game; a desktop's main() calls it, and so does
// Android's SupersonicMain (game/android/AndroidMain.cpp), which first unpacks the APK's data and builds the argument list a command line would
// have carried. One definition, so the two cannot drift: nothing about a desktop run changed when this was split out of main().
//
// argv[0] and the flags, as main() receives them. Returns the process's exit code: 0 for a run that ended clean, 1 for any failure (the reason is
// in the log and on stderr, and in a message box for a player's run). Never throws.
int MagicPortalsMain(int argc, char** argv);
