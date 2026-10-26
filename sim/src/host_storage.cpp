#include "host_storage.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

#include "portable.h"

namespace fs = std::filesystem;

namespace sim {

namespace {
bool isDir(const std::string& p) {
    std::error_code ec;
    return fs::is_directory(u8Path(p), ec);
}
[[maybe_unused]] bool pathExists(const std::string& p) {   // case-sensitive hosts only
    std::error_code ec;
    return fs::exists(u8Path(p), ec);
}
bool mkdirs(const std::string& p) {
    if (p.empty() || isDir(p)) return true;
    std::error_code ec;
    fs::create_directories(u8Path(p), ec);
    return isDir(p);
}
bool makeDir(const std::string& p) {
    std::error_code ec;
    return fs::create_directory(u8Path(p), ec) || isDir(p);
}
}  // namespace

bool HostStorage::init(const std::string& root) {
    root_ = root;
    while (root_.size() > 1 && (root_.back() == '/' || root_.back() == '\\')) root_.pop_back();
    return mkdirs(root_) && isDir(root_);
}

bool HostStorage::resolve(const char* path, std::string& out, bool* existsOut) const {
    out = root_;
    bool exists = true;
    std::string p(path ? path : "");
    size_t i = 0;
    while (i < p.size()) {
        while (i < p.size() && p[i] == '/') ++i;
        if (i >= p.size()) break;
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        const std::string comp = p.substr(i, j - i);
        i = j;
        if (comp == "." ) continue;
        if (comp == "..") return false;   // not on a FAT card root
        if (!exists) return false;        // missing intermediate directory
        std::string cand = out + "/" + comp;
#if !defined(_WIN32) && !defined(__APPLE__)
        // Case-sensitive host FS: an existing path is the exact name.
        if (pathExists(cand)) {
            out = cand;
            continue;
        }
#endif
        // FAT semantics: case-insensitive match. On a case-insensitive host FS
        // (Windows, macOS) this also picks up the on-disk spelling, so two
        // spellings of one card path resolve to the same host path.
        bool found = false;
        std::error_code ec;
        for (fs::directory_iterator it(u8Path(out), ec), end; !ec && it != end; it.increment(ec)) {
            const std::string n = pathU8(it->path().filename());
            if (strcaseCmp(n.c_str(), comp.c_str()) == 0) {
                out += "/";
                out += n;
                found = true;
                break;
            }
        }
        if (!found) {
            out = cand;
            exists = false;
        }
    }
    if (existsOut) *existsOut = exists;
    return true;
}

ot::IStorage::Handle HostStorage::open(const char* path, OpenMode mode) {
    if (refuse()) return -1;
    std::string hp;
    bool ex = false;
    if (!resolve(path, hp, &ex)) return -1;
    if (ex && isDir(hp)) return -1;
    int slot = -1;
    for (int i = 0; i < kMaxOpen; ++i)
        if (!files_[i]) { slot = i; break; }
    if (slot < 0) return -1;
    std::FILE* f = nullptr;
    switch (mode) {
        case kRead: f = ex ? openFile(hp, "rb") : nullptr; break;
        case kWrite: f = openFile(hp, "wb"); break;
        case kReadWrite: f = openFile(hp, ex ? "r+b" : "w+b"); break;
    }
    if (!f) return -1;
    files_[slot] = f;
    return slot;
}

int32_t HostStorage::read(Handle h, void* dst, uint32_t len) {
    if (refuse()) return -1;
    std::FILE* f = get(h);
    if (!f) return -1;
    std::fflush(f);
    const size_t n = std::fread(dst, 1, len, f);
    if (n < len && std::ferror(f)) return -1;
    return int32_t(n);
}

int32_t HostStorage::write(Handle h, const void* src, uint32_t len) {
    if (refuse()) return -1;
    std::FILE* f = get(h);
    if (!f) return -1;
    // Switching from reading to writing on a "r+" stream needs a positioning call.
    std::fseek(f, 0, SEEK_CUR);
    const size_t n = std::fwrite(src, 1, len, f);
    return n == len || n > 0 ? int32_t(n) : -1;
}

bool HostStorage::seek(Handle h, uint32_t pos) {
    if (refuse()) return false;
    std::FILE* f = get(h);
    return f && std::fseek(f, long(pos), SEEK_SET) == 0;
}

int32_t HostStorage::size(Handle h) {
    if (refuse()) return -1;
    std::FILE* f = get(h);
    if (!f) return -1;
    // Card files are < 2 GiB (FAT32 / int32_t sizes), so long is enough everywhere.
    const long cur = std::ftell(f);
    if (cur < 0 || std::fseek(f, 0, SEEK_END) != 0) return -1;
    const long end = std::ftell(f);
    std::fseek(f, cur, SEEK_SET);
    return end < 0 ? -1 : int32_t(end);
}

void HostStorage::close(Handle h) {
    if (refuse()) return;
    if (std::FILE* f = get(h)) {
        std::fclose(f);
        files_[h] = nullptr;
    }
}

bool HostStorage::exists(const char* path) {
    if (refuse()) return false;
    std::string hp;
    bool ex = false;
    return resolve(path, hp, &ex) && ex;
}

bool HostStorage::mkdir(const char* path) {
    if (refuse()) return false;
    std::string hp;
    bool ex = false;
    if (!resolve(path, hp, &ex)) return false;
    if (ex) return isDir(hp);
    return makeDir(hp);
}

bool HostStorage::remove(const char* path) {
    if (refuse()) return false;
    std::string hp;
    bool ex = false;
    if (!resolve(path, hp, &ex) || !ex || hp == root_) return false;
    // Files, or empty directories only (like f_unlink).
    std::error_code ec;
    return fs::remove(u8Path(hp), ec) && !ec;
}

bool HostStorage::rename(const char* from, const char* to) {
    if (refuse()) return false;
    std::string a, b;
    bool exA = false, exB = false;
    if (!resolve(from, a, &exA) || !exA || !resolve(to, b, &exB)) return false;
    if (exB && a != b) return false;   // FatFs f_rename fails if the target exists
    std::error_code ec;
    fs::rename(u8Path(a), u8Path(b), ec);
    return !ec;
}

int32_t HostStorage::listDir(const char* path, DirFn fn, void* ctx) {
    if (refuse()) return -1;
    std::string hp;
    bool ex = false;
    if (!resolve(path, hp, &ex) || !ex || !isDir(hp)) return -1;
    struct Ent {
        std::string name;
        bool dir;
        uint32_t size;
    };
    std::vector<Ent> ents;
    std::error_code ec;
    fs::directory_iterator it(u8Path(hp), ec);
    if (ec) return -1;
    for (fs::directory_iterator end; it != end; it.increment(ec)) {
        if (ec) break;
        std::error_code e2;
        const bool dir = it->is_directory(e2);
        if (e2) continue;
        uint32_t sz = 0;
        if (!dir) {
            const auto s = it->file_size(e2);
            if (e2) continue;
            sz = uint32_t(s);
        }
        ents.push_back({pathU8(it->path().filename()), dir, sz});
    }
    std::sort(ents.begin(), ents.end(), [](const Ent& x, const Ent& y) { return x.name < y.name; });
    int32_t n = 0;
    for (const Ent& e : ents) {
        DirEntry de{e.name.c_str(), e.dir, e.size};
        ++n;
        if (fn && !fn(de, ctx)) break;
    }
    return n;
}

}  // namespace sim
