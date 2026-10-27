#include "host_os.h"

#include <cstdio>

#include "portable.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace sim {

#ifdef _WIN32

bool outputHasNowhereToGo() {
    const HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    return h == nullptr || h == INVALID_HANDLE_VALUE || GetFileType(h) == FILE_TYPE_UNKNOWN;
}

bool redirectOutputTo(const std::string& path) {
    const std::wstring w = u8Path(path).wstring();
    // Start an empty file, then both streams append, so neither overwrites
    // what the other wrote.
    std::FILE* f = _wfopen(w.c_str(), L"w");
    if (!f) return false;
    std::fclose(f);
    if (!_wfreopen(w.c_str(), L"a", stdout)) return false;
    if (!_wfreopen(w.c_str(), L"a", stderr)) return false;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}

std::string executableDir() {
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0])) return {};
    std::string dir = pathU8(std::filesystem::path(std::wstring(buf, n)).parent_path());
    for (char& c : dir)
        if (c == '\\') c = '/';
    return dir;
}

#else

bool outputHasNowhereToGo() { return false; }
bool redirectOutputTo(const std::string&) { return false; }
std::string executableDir() { return {}; }

#endif

}  // namespace sim
