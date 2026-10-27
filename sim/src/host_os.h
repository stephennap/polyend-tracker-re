// OpenTracker simulator -- small OS queries kept out of main.cpp (Windows
// headers stay in host_os.cpp). On Linux/macOS they are no-ops.
#pragma once

#include <string>

namespace sim {

// True when stdout/stderr lead nowhere: the windowed Windows build
// (OpenTracker.exe, GUI subsystem) started from Explorer or a shortcut.
// Always false for console programs and when output is redirected.
bool outputHasNowhereToGo();

// Sends stdout and stderr to `path` (UTF-8, overwritten), unbuffered so the
// file is current while the simulator runs. False if it cannot be opened.
bool redirectOutputTo(const std::string& path);

// Folder of the running executable (UTF-8, '/' separators), or "" if unknown.
std::string executableDir();

}  // namespace sim
