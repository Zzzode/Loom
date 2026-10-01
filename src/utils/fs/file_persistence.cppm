module;

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

export module loom.fs.file_persistence;

import std;

namespace fs = std::filesystem;

export namespace cc::utils {

namespace detail {

// Call ::fsync() on a POSIX file descriptor.  Returns empty string on
// success, or a human-readable diagnostic on failure.  Best-effort: if the
// platform doesn't support fsync on this fd type (e.g. /dev/null, some
// network filesystems), we log-and-continue by returning empty string too,
// since a subsequent rename() would fail visibly anyway if the underlying
// filesystem was truly broken.
inline auto fsync_fd(int fd) -> std::string {
    if (fd < 0) return "invalid fd";
#if defined(__APPLE__) || defined(__linux__)
    // On macOS, fcntl(F_FULLFSYNC) asks the drive to actually flush its
    // write cache, which is stronger than fsync() alone.  Fall through to
    // plain fsync on failure.
#if defined(__APPLE__)
    if (fcntl(fd, F_FULLFSYNC) == 0) return {};
#endif
    if (::fsync(fd) == 0) return {};
    // EINVAL is common on pseudo filesystems / pipes — treat as success.
    if (errno == EINVAL) return {};
    char buf[128];
    std::snprintf(buf, sizeof(buf), "fsync failed (errno=%d)", errno);
    return std::string(buf);
#else
    (void)fd;
    return {};  // non-POSIX: nothing reliable we can do
#endif
}

// Open a path for fsync-of-directory / fsync-of-file.  Returns -1 on
// failure.  Caller must close() the returned fd.
[[nodiscard]] inline auto open_for_fsync(const fs::path& p, bool is_dir) -> int {
#if defined(__APPLE__) || defined(__linux__)
    const int flags = O_RDONLY | (is_dir ? O_DIRECTORY : 0);
    int fd = ::open(p.c_str(), flags);
    return fd;
#else
    (void)p; (void)is_dir;
    return -1;
#endif
}

}  // namespace detail

} // namespace cc::utils
