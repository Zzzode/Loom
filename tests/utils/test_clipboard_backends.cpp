/// @file test_clipboard_backends.cpp
/// @brief RFC 0007 Phase 1: Linux backend detection tests.
///
/// Verifies that resolve_native_backend() correctly resolves wl-copy/xclip
/// via PATH+X_OK lookup, and that xsel is excluded.

#include <cstdlib>
#include <gtest/gtest.h>

import std;
import loom.platform.clipboard;

namespace {

struct EnvGuard {
    std::string name;
    std::optional<std::string> previous;

    explicit EnvGuard(std::string key, std::string_view value)
        : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), std::string(value).c_str(), 1);
    }

    ~EnvGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

struct EnvUnsetGuard {
    std::string name;
    std::optional<std::string> previous;

    explicit EnvUnsetGuard(std::string key) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        unsetenv(name.c_str());
    }

    ~EnvUnsetGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

/// Create a temp directory with a fake executable.
struct FakeBin {
    std::filesystem::path dir;

    FakeBin() {
        dir = std::filesystem::temp_directory_path() /
              ("loom-test-bin-" + std::to_string(::getpid()));
        std::filesystem::create_directories(dir);
    }

    ~FakeBin() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    /// Create a fake executable with the given name.
    void create(std::string_view name) {
        auto path = dir / std::string(name);
        {
            std::ofstream f(path);
            f << "#!/bin/sh\nexit 0\n";
        }
        std::filesystem::permissions(path,
            std::filesystem::perms::owner_all |
            std::filesystem::perms::group_read |
            std::filesystem::perms::others_read,
            std::filesystem::perm_options::replace);
    }
};

}  // namespace

TEST(ClipboardBackends, NoBackendReturnsNullopt) {
    // Set PATH to a temp dir with no clipboard tools.
    FakeBin bin;
    EnvGuard path("PATH", bin.dir.string());
    EnvUnsetGuard wayland("WAYLAND_DISPLAY");
    EnvUnsetGuard ssh_conn("SSH_CONNECTION");
    EnvUnsetGuard ssh_tty("SSH_TTY");

    auto backend = loom::utils::clipboard::resolve_native_backend();
    EXPECT_FALSE(backend.has_value());
}

TEST(ClipboardBackends, WaylandPrefersWlCopy) {
    FakeBin bin;
    bin.create("wl-copy");
    bin.create("xclip");
    EnvGuard path("PATH", bin.dir.string());
    EnvGuard wayland("WAYLAND_DISPLAY", "wayland-0");
    EnvUnsetGuard ssh_conn("SSH_CONNECTION");
    EnvUnsetGuard ssh_tty("SSH_TTY");

    auto backend = loom::utils::clipboard::resolve_native_backend();
    ASSERT_TRUE(backend.has_value());
    EXPECT_EQ(backend->argv[0], "wl-copy");
}

TEST(ClipboardBackends, X11UsesXclip) {
    FakeBin bin;
    bin.create("xclip");
    EnvGuard path("PATH", bin.dir.string());
    EnvUnsetGuard wayland("WAYLAND_DISPLAY");
    EnvUnsetGuard ssh_conn("SSH_CONNECTION");
    EnvUnsetGuard ssh_tty("SSH_TTY");

    auto backend = loom::utils::clipboard::resolve_native_backend();
    ASSERT_TRUE(backend.has_value());
    EXPECT_EQ(backend->argv[0], "xclip");
    // xclip must target CLIPBOARD, not PRIMARY.
    auto it = std::find(backend->argv.begin(), backend->argv.end(),
                        "-selection");
    ASSERT_NE(it, backend->argv.end());
    EXPECT_EQ(*(it + 1), "clipboard");
}

TEST(ClipboardBackends, XselExcluded) {
    // Even if xsel is the only tool available, it must NOT be used.
    FakeBin bin;
    bin.create("xsel");
    EnvGuard path("PATH", bin.dir.string());
    EnvUnsetGuard wayland("WAYLAND_DISPLAY");
    EnvUnsetGuard ssh_conn("SSH_CONNECTION");
    EnvUnsetGuard ssh_tty("SSH_TTY");

    auto backend = loom::utils::clipboard::resolve_native_backend();
    EXPECT_FALSE(backend.has_value());
}

TEST(ClipboardBackends, WaylandWithoutWlCopyFallsBackToXclip) {
    FakeBin bin;
    bin.create("xclip");
    EnvGuard path("PATH", bin.dir.string());
    EnvGuard wayland("WAYLAND_DISPLAY", "wayland-0");
    EnvUnsetGuard ssh_conn("SSH_CONNECTION");
    EnvUnsetGuard ssh_tty("SSH_TTY");

    auto backend = loom::utils::clipboard::resolve_native_backend();
    ASSERT_TRUE(backend.has_value());
    EXPECT_EQ(backend->argv[0], "xclip");
}
