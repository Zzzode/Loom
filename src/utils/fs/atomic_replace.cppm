module;

// GMF policy (RFC-0001 B c16): POSIX/Opaque-C headers ONLY. Including
// libc++ C++ headers (cstdio/cstdlib/cstring/...) here while `import std`
// sits in the module purview duplicates the std module's operator-new
// declarations in a cold implicit-module cache ("call to 'operator new' is
// ambiguous" in downstream impl units); the std module supplies every C++
// facility used below. errno/mode_t/open/fsync come from the opaque C
// headers, which never declare C++ allocation functions.
#include <cerrno>
#include <fcntl.h>
#include <stdio.h>  // rename (opaque C header, not <cstdio>)
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <stdlib.h>  // arc4random_buf (opaque C header, not <cstdlib>)
#endif

export module cc.utils.atomic_replace;

import std;
import cc.utils.file_persistence;  // detail::open_for_fsync / fsync_fd

namespace fs = std::filesystem;

export namespace cc::utils {

/// Permission policy for atomic_replace_file().
enum class AtomicMode {
    /// Replacing an existing file preserves its mode bits; a brand-new file
    /// is created 0666 so the process umask applies exactly as it did for
    /// the old truncating std::ofstream (0644 under umask 022).
    PreserveOrUmask,
    /// The result is always owner-only 0600 (secret-bearing stores).
    OwnerOnly,
};

namespace atomic_replace_detail {

/// 16 hex chars (64 bits) of tmp-name randomness, so pre-spraying symlinks
/// at guessed "<path>.tmp.<pid>.<n>" names cannot exhaust the O_EXCL retry
/// loop. arc4random where the platform ships it; random_device +
/// mt19937_64 elsewhere (same shape as the config-layer hardened writer).
[[nodiscard]] inline std::string random_tmp_suffix() {
    std::uint64_t value = 0;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    ::arc4random_buf(&value, sizeof(value));
#else
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    value = rng();
#endif
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(16);
    for (int shift = 60; shift >= 0; shift -= 4) {
        out.push_back(hex[(value >> shift) & 0x0F]);
    }
    return out;
}

}  // namespace atomic_replace_detail

/// Atomically replace `path` with `content` with hardened non-regular-file
/// semantics (c16; mechanics copied, not refactored, from the C6-byte-pinned
/// write_config_file_replace in src/config/config.cppm).
///
/// Sequence: parent mkdir (ec-checked); lstat() the leaf — an existing
/// target MUST be a regular file (a symlink, FIFO, socket or device is a
/// specific refusal: the old truncating std::ofstream followed symlinks,
/// clobbered symlink victims, and BLOCKED opening a FIFO); write goes to a
/// unique tmp "<path>.tmp.<pid>.<counter>.<16hex>" opened
/// O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC (EEXIST/ELOOP -> fresh
/// random name, up to 32 tries); fchmod() applies the mode BEFORE any data
/// lands; writes are EINTR-restarted; fsync(file); close; rename(2); then a
/// best-effort fsync of the parent directory. Every failure path unlinks
/// the tmp. Readers of the target see either the old inode or the new one,
/// never a half-written file.
///
/// ADVISORY LOCKING IS ORTHOGONAL. This function takes no flock: callers
/// that do read-modify-write serialize themselves with LOCK_EX on a
/// "<path>.lock" SIBLING (cc::utils::ScopedFileLock), and readers take
/// LOCK_SH on the same sibling. rename(2) replaces the DATA inode but the
/// lock lives on the sibling inode, which is never renamed, so SH/EX keep
/// serializing across arbitrarily many data-inode replacements.
inline std::expected<void, std::string> atomic_replace_file(
    const fs::path& path,
    std::string_view content,
    AtomicMode mode = AtomicMode::PreserveOrUmask
) {
    auto fail = [&](std::string message)
        -> std::expected<void, std::string> {
        return std::unexpected(std::move(message));
    };

    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            return fail("Cannot create parent directory for " +
                        path.string() + ": " + ec.message());
        }
    }

    // Leaf gate: only an absent path or a REGULAR file may be replaced.
    // lstat() never follows a trailing symlink; O_NOFOLLOW below only
    // protects the tmp name, so this check is what protects the data leaf.
    struct ::stat leaf_st {};
    bool leaf_present = false;
    mode_t existing_mode = 0;
    if (::lstat(path.c_str(), &leaf_st) == 0) {
        if (S_ISLNK(leaf_st.st_mode)) {
            return fail(path.string() +
                        " is a symbolic link; refusing to write through a "
                        "symlinked data file");
        }
        if (!S_ISREG(leaf_st.st_mode)) {
            return fail(path.string() +
                        " is not a regular file; refusing to replace a "
                        "FIFO, socket, device or directory data leaf");
        }
        leaf_present = true;
        existing_mode = leaf_st.st_mode & 07777;
    } else if (errno != ENOENT) {
        return fail("Cannot stat data file " + path.string() + ": " +
                    std::error_code(errno, std::generic_category()).message());
    }

    const bool owner_only = (mode == AtomicMode::OwnerOnly);
    const bool preserve_mode =
        !owner_only && leaf_present;

    static std::atomic<std::uint64_t> tmp_counter{0};
    constexpr int kMaxTmpAttempts = 32;
    for (int attempt = 0; attempt < kMaxTmpAttempts; ++attempt) {
        // pid + in-process counter + 64 bits of randomness: guessing a name
        // in advance (to pre-place a symlink) is impractical, and an
        // attacker who does race the exact name loses into EEXIST/ELOOP and
        // this loop simply picks another.
        const fs::path temp =
            path.string() + ".tmp." + std::to_string(::getpid()) + "." +
            std::to_string(tmp_counter.fetch_add(1,
                std::memory_order_relaxed)) + "." +
            atomic_replace_detail::random_tmp_suffix();

        // 0600 base for mode-managed writes (fchmod sets the exact mode
        // next); 0666 for a brand-new default-mode file so the umask is
        // applied by the kernel exactly as ofstream did (0644 @ 022).
        const mode_t open_mode =
            (owner_only || preserve_mode) ? 0600 : 0666;
        const int fd = ::open(temp.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW |
                                  O_CLOEXEC,
                              open_mode);
        if (fd < 0) {
            if (errno == EEXIST || errno == ELOOP) continue;
            return fail("Cannot create temporary file for " +
                        path.string() + ": " + std::error_code(errno, std::generic_category()).message());
        }

        // Apply the mode BEFORE data lands and before the rename.
        if (owner_only) {
            if (::fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
                // A failed hardening attempt fails closed rather than leave
                // a wider-than-intended mode.
                const int saved_errno = errno;
                std::error_code remove_ec;
                fs::remove(temp, remove_ec);
                (void)::close(fd);
                return fail("Cannot restrict permissions on " +
                            temp.string() + ": " +
                            std::error_code(saved_errno, std::generic_category()).message());
            }
        } else if (preserve_mode) {
            // Keep the pre-existing inode's mode across the replace; a
            // failure leaves the stricter 0600 base, so make it fatal
            // rather than silently change the file's mode.
            if (::fchmod(fd, existing_mode) != 0) {
                const int saved_errno = errno;
                std::error_code remove_ec;
                fs::remove(temp, remove_ec);
                (void)::close(fd);
                return fail("Cannot preserve permissions on " +
                            temp.string() + ": " +
                            std::error_code(saved_errno, std::generic_category()).message());
            }
        }

        bool write_ok = true;
        int saved_errno = 0;
        std::size_t total = 0;
        while (total < content.size()) {
            const ssize_t written = ::write(
                fd, content.data() + total, content.size() - total);
            if (written < 0) {
                if (errno == EINTR) continue;
                write_ok = false;
                saved_errno = errno;
                break;
            }
            total += static_cast<std::size_t>(written);
        }
        if (write_ok && ::fsync(fd) != 0 && errno != EINVAL) {
            write_ok = false;  // EINVAL: pseudo fs, treat as best-effort
            saved_errno = errno;
        }
        if (write_ok && ::close(fd) != 0) {
            write_ok = false;
            saved_errno = errno;
        } else if (!write_ok) {
            (void)::close(fd);
        }

        if (!write_ok) {
            std::error_code remove_ec;
            fs::remove(temp, remove_ec);
            return fail("Failed to write temporary file for " +
                        path.string() + ": " +
                        std::error_code(saved_errno, std::generic_category()).message());
        }

        if (::rename(temp.c_str(), path.c_str()) == 0) {
            // Durability: fsync the parent directory so the rename is
            // stable; best-effort (mirrors cc::utils::atomic_write).
            if (!parent.empty()) {
                const int dir_fd =
                    detail::open_for_fsync(parent, /*is_dir=*/true);
                if (dir_fd >= 0) {
                    (void)detail::fsync_fd(dir_fd);
                    ::close(dir_fd);
                }
            }
            return {};
        }

        const int rename_errno = errno;
        std::error_code remove_ec;
        fs::remove(temp, remove_ec);
        return fail("Cannot atomically replace " + path.string() + ": " +
                    std::error_code(rename_errno, std::generic_category()).message());
    }
    // Exhausted unique names (an attacker racing every O_EXCL create).
    return fail("Cannot create a unique temporary file for " +
                path.string());
}

/// Outcome of read_regular_file().
enum class RegularReadStatus : std::uint8_t {
    /// A REGULAR file was opened through the O_NOFOLLOW fd and fully read;
    /// RegularFileRead::contents holds its bytes.
    Present,
    /// No file exists at the path (open failed ENOENT).
    Absent,
    /// The leaf is a symlink/FIFO/socket/device, a non-regular inode raced
    /// in between open and fstat, or the read itself failed. Nothing was
    /// followed and the call never blocked.
    Unreadable,
};

struct RegularFileRead {
    RegularReadStatus status = RegularReadStatus::Unreadable;
    std::string contents;

    [[nodiscard]] bool present() const noexcept {
        return status == RegularReadStatus::Present;
    }
};

/// Hardened read counterpart to atomic_replace_file(): open `path` ONCE and
/// read through that fd, so a same-uid actor cannot swap a FIFO or symlink
/// over the leaf between a stat gate and a path-based reopen (the TOCTOU
/// behind lstat()+yyjson_read_file: the reopen would follow the link or, on
/// a FIFO, BLOCK with no peer and no timeout).
///
///   * O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC: O_NOFOLLOW rejects a
///     trailing symlink with ELOOP; O_NONBLOCK makes opening a FIFO (with no
///     writer peer) return immediately instead of blocking — the fstat gate
///     below then rejects it without a single read.
///   * fstat(2) on the OPEN descriptor proves the inode is S_ISREG, closing
///     the swap race even if the directory entry changed after open.
///   * ENOENT maps to Absent; every other failure to Unreadable.
///   * The read loop is EINTR-restarted. For a regular file POSIX requires
///     O_NONBLOCK reads to behave exactly like blocking reads — EAGAIN can
///     never occur (Linux explicitly ignores O_NONBLOCK on regular fds); a
///     defensive bounded retry fails closed rather than spin if that
///     guarantee were ever violated.
inline RegularFileRead read_regular_file(const fs::path& path) {
    RegularFileRead result;
    const int fd = ::open(path.c_str(),
                          O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        result.status = (errno == ENOENT) ? RegularReadStatus::Absent
                                          : RegularReadStatus::Unreadable;
        return result;
    }

    struct ::stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        (void)::close(fd);
        result.status = RegularReadStatus::Unreadable;
        return result;
    }

    std::string& out = result.contents;
    if (st.st_size > 0) {
        out.reserve(static_cast<std::size_t>(
            std::min<std::int64_t>(st.st_size, 64 * 1024 * 1024)));
    }
    constexpr std::size_t kChunk = 64 * 1024;
    int eagain_strikes = 0;
    for (;;) {
        char buffer[kChunk];
        const ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            out.append(buffer, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) break;  // EOF
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Unreachable for a proven-regular fd (see function comment);
            // bound the defensive retry and fail closed.
            if (++eagain_strikes > 100) {
                (void)::close(fd);
                result.status = RegularReadStatus::Unreadable;
                result.contents.clear();
                return result;
            }
            continue;
        }
        (void)::close(fd);
        result.status = RegularReadStatus::Unreadable;
        result.contents.clear();
        return result;
    }

    if (::close(fd) != 0) {
        result.status = RegularReadStatus::Unreadable;
        result.contents.clear();
        return result;
    }
    result.status = RegularReadStatus::Present;
    return result;
}

}  // namespace cc::utils
