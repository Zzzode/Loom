// Test-only macOS CA injection for the real Loom PTY child. cpp-httplib loads
// Keychain certificates instead of OpenSSL's SSL_CERT_FILE on macOS. Add only
// the fixture CA before SSL_new; keep verification, sockets and signals intact.
#include <openssl/ssl.h>

#include <cstdlib>

namespace {

SSL* new_ssl_with_fixture_ca(SSL_CTX* context) {
    const char* certificate = std::getenv("LOOM_PTY_TLS_CA_FILE");
    if (certificate && *certificate) {
        if (SSL_CTX_load_verify_locations(context, certificate, nullptr) != 1) {
            return nullptr;
        }
    }
    return SSL_new(context);
}

struct Interpose {
    const void* replacement;
    const void* original;
};

__attribute__((used, section("__DATA,__interpose"))) const Interpose ca_interpose{
    reinterpret_cast<const void*>(new_ssl_with_fixture_ca),
    reinterpret_cast<const void*>(SSL_new),
};

}  // namespace
