/// @file test_clipboard_ssh.cpp
/// @brief RFC 0007 Phase 1: SSH detection tests.
///
/// Verifies that is_ssh_session() correctly detects SSH_CONNECTION and
/// SSH_TTY environment variables, and that resolve_native_backend()
/// returns nullopt on SSH sessions.

#include <gtest/gtest.h>

#include <cstdlib>

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

    explicit EnvUnsetGuard(std::string key)
        : name(std::move(key)) {
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

}  // namespace

TEST(ClipboardSsh, NoSshEnvReturnsFalse) {
    EnvUnsetGuard conn("SSH_CONNECTION");
    EnvUnsetGuard tty("SSH_TTY");
    EXPECT_FALSE(loom::utils::clipboard::is_ssh_session());
}

TEST(ClipboardSsh, SshConnectionDetected) {
    EnvUnsetGuard tty("SSH_TTY");
    EnvGuard conn("SSH_CONNECTION", "192.168.1.1 12345 10.0.0.1 22");
    EXPECT_TRUE(loom::utils::clipboard::is_ssh_session());
}

TEST(ClipboardSsh, SshTtyDetected) {
    EnvUnsetGuard conn("SSH_CONNECTION");
    EnvGuard tty("SSH_TTY", "/dev/pts/0");
    EXPECT_TRUE(loom::utils::clipboard::is_ssh_session());
}

TEST(ClipboardSsh, BothSshEnvDetected) {
    EnvGuard conn("SSH_CONNECTION", "192.168.1.1 12345 10.0.0.1 22");
    EnvGuard tty("SSH_TTY", "/dev/pts/0");
    EXPECT_TRUE(loom::utils::clipboard::is_ssh_session());
}

TEST(ClipboardSsh, BackendResolutionSkippedOnSsh) {
    EnvGuard conn("SSH_CONNECTION", "192.168.1.1 12345 10.0.0.1 22");
    // On SSH, resolve_native_backend should return nullopt regardless of
    // whether backends are available.
    auto backend = loom::utils::clipboard::resolve_native_backend();
    EXPECT_FALSE(backend.has_value());
}
