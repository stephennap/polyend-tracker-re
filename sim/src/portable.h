// OpenTracker simulator -- small helpers that keep the host code the same on
// Linux, macOS and Windows (MinGW). Paths are UTF-8 std::strings everywhere;
// on Windows they are converted to wide strings at the OS boundary so non-ASCII
// card file names work.
#pragma once

#include <cstdio>
#include <filesystem>
#include <string>

namespace sim {

// ASCII case-insensitive compare (strcasecmp is POSIX only).
inline int strcaseCmp(const char* a, const char* b) {
    for (;; ++a, ++b) {
        unsigned char x = static_cast<unsigned char>(*a), y = static_cast<unsigned char>(*b);
        if (x >= 'A' && x <= 'Z') x = static_cast<unsigned char>(x + ('a' - 'A'));
        if (y >= 'A' && y <= 'Z') y = static_cast<unsigned char>(y + ('a' - 'A'));
        if (x != y || x == 0) return int(x) - int(y);
    }
}

// UTF-8 string <-> filesystem path.
inline std::filesystem::path u8Path(const std::string& s) { return std::filesystem::u8path(s); }
inline std::string pathU8(const std::filesystem::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

// fopen with a UTF-8 path.
inline std::FILE* openFile(const std::string& path, const char* mode) {
#ifdef _WIN32
    std::wstring wmode(mode, mode + std::char_traits<char>::length(mode));
    return ::_wfopen(u8Path(path).c_str(), wmode.c_str());
#else
    return std::fopen(path.c_str(), mode);
#endif
}

}  // namespace sim
