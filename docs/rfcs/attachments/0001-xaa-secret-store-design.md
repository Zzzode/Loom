# RFC 0001 follow-up c17a (deferred item) — XAA IdP client secret: single store via the composition seam

Status: design proposal, pre-implementation. Date 2026-09-29.

This document designs the fix for the open item recorded at the end of the
c17/c17a/c18 entry in RFC 0001 section 12:

> the `--xaa` runtime path still sources `XaaConfig::idp_client_secret` from
> the dead `xaa-idp.txt` line rather than the store — the same asymmetry
> closed for the port persists for that one field, and closing it changes the
> file-config contract.

The port asymmetry was closed in c17a by making
`settings.xaaIdp.callbackPort` the single source, injected through the
`core_settings_loader` composition seam. This design closes the same
asymmetry for `idp_client_secret`, making the hardened credential store
(`~/.config/loom/xaa/idp_tokens.json`) the single source, injected through
the same seam.

## Problem

The `--xaa` runtime path (`mcp_auth` tool → `perform_mcp_oauth_flow` →
`get_xaa_config` → `read_xaa_config_file`) reads the IdP client secret from
`~/.loom/xaa-idp.txt` — a file that **nothing in `src/` ever writes** (only
tests create it, `tests/test_services.cpp:5121`). The supported CLI surface
(`/mcp xaa setup --client-secret`, `/mcp add --xaa --client-secret`) writes
the secret to the hardened credential store
(`~/.config/loom/xaa/idp_tokens.json`) via `save_idp_client_secret()`. Two
stores, no linkage: a secret stored through the supported CLI never reaches
the `--xaa` runtime path, and a secret hand-edited into `xaa-idp.txt` is the
only one that does.

This is the exact asymmetry c17a closed for `callback_port`, applied to
`idp_client_secret`.

## (a) Current-state map — every `idp_client_secret` read/write site

### The two stores

| Store | Path | Written by | Mode |
|---|---|---|---|
| **Hardened (live)** | `~/.config/loom/xaa/idp_tokens.json` | `save_idp_client_secret()` (`xaa_idp_login.cppm:410`) | 0600 file / 0700 dir (c18) |
| **Dead** | `~/.loom/xaa-idp.txt` | nothing in `src/` (hand-edited only) | none (umask, typically 0644) |

The hardened store holds the secret under `mcpXaaIdpConfig.<issuer_key>.clientSecret`
(`xaa_idp_login.cppm:312-316`), keyed by the normalized issuer
(`issuer_key()`, `xaa_idp_login.cppm:114`).

### Read sites

| # | Site | File:line | Store | Status |
|---|---|---|---|---|
| R1 | `read_xaa_config_file()` parses `idp_client_secret=` line | `xaa.cppm:802-803` | dead `xaa-idp.txt` | **dead** |
| R2 | `get_xaa_config()` calls `read_xaa_config_file()` | `xaa.cppm:855-858` | dead `xaa-idp.txt` | **dead** |
| R3 | `perform_mcp_oauth_flow()` calls `get_xaa_config()` on the `--xaa` path | `auth.cppm:794` | dead `xaa-idp.txt` (via R1/R2) | **dead** |
| R4 | `read_idp_client_secret()` (detail) | `xaa_idp_login.cppm:324-345` | hardened store | live |
| R5 | `get_idp_client_secret()` (public) | `xaa_idp_login.cppm:404-407` | hardened store | live |
| R6 | `resolve_login_client_secret()` | `xaa_idp_login.cppm:999-1004` | hardened store | live |
| R7 | `build_login_options()` folds R6 into `IdpLoginOptions` | `xaa_idp_login.cppm:1032` | hardened store | live |
| R8 | `read_xaa_idp_status()` (presence-only `has_client_secret`) | `mcp_cmd.cppm:440` | hardened store | live |

### Write sites

| # | Site | File:line | Store | Status |
|---|---|---|---|---|
| W1 | `write_idp_client_secret()` (detail) | `xaa_idp_login.cppm:289-322` | hardened store | live |
| W2 | `save_idp_client_secret()` (public) | `xaa_idp_login.cppm:410-414` | hardened store | live |
| W3 | `acquire_idp_id_token()` persists secret after login | `xaa_idp_login.cppm:983-984` | hardened store | live |
| W4 | `execute_xaa_setup()` `--client-secret` | `mcp_cmd.cppm:1062` | hardened store | live |
| W5 | `execute_add()` `--xaa --client-secret` | `mcp_cmd.cppm:691-692` | hardened store | live |
| W6 | `remove_idp_client_secret()` (detail) | `xaa_idp_login.cppm:348-368` | hardened store | live |
| W7 | `clear_idp_client_secret()` (public) | `xaa_idp_login.cppm:417-419` | hardened store | live |
| W8 | `execute_xaa_clear()` | `mcp_cmd.cppm:1164` | hardened store | live |

### Consume sites (where the secret value is used)

| # | Site | File:line | Purpose |
|---|---|---|---|
| C1 | `authenticate_xaa()` sets `login_opts.idp_client_secret` | `xaa.cppm:906` | OIDC auth_code+PKCE login leg (basic/post auth at the IdP token endpoint) |
| C2 | `perform_cross_app_access()` passes `config.idp_client_secret` to `request_jwt_authorization_grant()` | `xaa.cppm:711-712` | RFC 8693 token-exchange leg at the IdP (`client_secret_post`) |

### The asymmetry

- **`/mcp xaa login` path** (`perform_xaa_login` → `build_login_options` →
  `resolve_login_client_secret`): reads from the **hardened store** (R6/R7).
  Fixed in c17a.
- **`--xaa` runtime path** (`perform_mcp_oauth_flow` → `get_xaa_config` →
  `read_xaa_config_file`): reads from the **dead store** (R1/R2/R3).
  **This is the deferred item.**

Every write site (W1–W8) targets the hardened store. The only reader of the
dead store is the `--xaa` runtime path. Closing the asymmetry means the
`--xaa` runtime path reads from the hardened store too, and the dead
`idp_client_secret=` line is no longer parsed.

## (b) Target — single source, injected through the c17a composition seam

### The c17a seam (as built for the port)

The port is injected through this chain (all file:line cited):

1. **Loader slot**: `detail::core_settings_mcp_loader_slot()`
   (`mcp_tool.cppm:869`; storage in `mcp_core_settings_loader.cpp:23`).
2. **Layer struct**: `CoreSettingsMcpLayer` (`mcp_tool.cppm:858-861`),
   currently `{ servers, xaa_callback_port }`.
3. **Loader install**: `install_core_settings_mcp_loader()`
   (`core_settings_loader.cppm:35-54`) — the one composition root that may
   see both `cc.config.config` and `cc.orchestration.tools.mcp`.
4. **Runtime accessor**: `NativeMcpRuntime::xaa_callback_port()`
   (`mcp_tool.cppm:1072-1079`) — fresh per lookup, calls the loader without
   `mutex_` held.
5. **Free function**: `native_mcp_xaa_callback_port()`
   (`mcp_tool.cppm:1364-1366`).
6. **Gate**: `xaa_login_callback_port_for(is_xaa, port)`
   (`mcp_tool.cppm:1374-1378`) — forwards only on the XAA path.
7. **Flow parameter**: `perform_mcp_oauth_flow(..., xaa_callback_port)`
   (`auth.cppm:764-770`).
8. **Auth parameter**: `authenticate_xaa(..., callback_port)`
   (`xaa.cppm:886-891`).

### The secret through the same seam

The secret follows the identical chain. The exact changes:

1. **Layer struct** — `CoreSettingsMcpLayer` gains one field:
   ```cpp
   std::optional<std::string> xaa_idp_client_secret = std::nullopt;
   ```
   (`mcp_tool.cppm:858-861`). This is the **loader slot** the task asks to
   name: the same `core_settings_mcp_loader_slot()`, extended with a secret
   field on the layer it yields.

2. **Loader install** — `install_core_settings_mcp_loader()`
   (`core_settings_loader.cppm:35-54`) gains, after the existing
   `layer.xaa_callback_port = ...` line:
   ```cpp
   layer.xaa_idp_client_secret =
       cc::services::mcp::get_idp_client_secret(
           config.settings().xaa_idp.issuer);
   ```
   This requires `import cc.services.mcp.xaa_idp_login;` in
   `core_settings_loader.cppm`. The loader becomes the one composition root
   that may see `cc.config.config`, `cc.orchestration.tools.mcp`, *and*
   `cc.services.mcp.xaa_idp_login` — it already bridges config →
   orchestration; the secret store is a services-layer facility it composes
   in, the same way it composes the config value. No layering violation:
   commands rank above services rank 7.

   The issuer key is `settings().xaa_idp.issuer` — the same value
   `/mcp xaa setup --issuer` writes (`mcp_cmd.cppm:1051`) and the same key
   W4/W5 store the secret under. When the issuer is empty (XAA not
   configured), `get_idp_client_secret("")` returns `nullopt` (the store
   lookup misses), so the field stays unset on the read path — no
   special-casing needed there. The **migration** in section (d) is
   different and DOES need a guard: it must run ONLY when
   `!settings().xaa_idp.issuer.empty()`. Unguarded,
   `save_idp_client_secret("", *legacy)` would write the secret under the
   empty key `mcpXaaIdpConfig."".clientSecret` — after which
   `get_idp_client_secret("")` no longer misses and the invariant above is
   self-defeating. Two consequences: (a) store pollution with a phantom
   `""` key entry; (b) `/mcp xaa clear` is silently undone —
   `execute_xaa_clear()` (`mcp_cmd.cppm:1154-1168`) removes the secret under
   the old issuer AND resets `settings.xaaIdp` to `{}` (issuer becomes
   empty), so the next loader call would re-read the hand-edited legacy
   line, store it under `""`, set `layer.xaa_idp_client_secret`, and the
   secret would resurface on the next XAA auth.

3. **Runtime accessor** — `NativeMcpRuntime` gains
   `xaa_idp_client_secret()`, a sibling of `xaa_callback_port()`
   (`mcp_tool.cppm:1072-1079`). Same fresh-per-lookup semantics: the loader
   is called on every `--xaa` auth, so a secret stored after the process
   started is visible without a restart. The loader is called without
   `mutex_` held (it does filesystem I/O).

4. **Free function** — `native_mcp_xaa_idp_client_secret()`
   (`mcp_tool.cppm`, sibling of `native_mcp_xaa_callback_port()` at
   line 1364).

5. **Gate** — the existing `is_xaa` gate in `mcp_auth`
   (`mcp_tool.cppm:1627-1628`) already guards the port. The secret is
   forwarded on the same XAA-only condition, through one pure seam that
   resolves both: `xaa_login_secrets_for(is_xaa, port, secret)`, committed
   here as the sibling of `xaa_login_callback_port_for`
   (`mcp_tool.cppm:1374-1378`) rather than left as an inline ternary:
   ```cpp
   const auto [xaa_port, xaa_secret] = xaa_login_secrets_for(
       configured->oauth->xaa,
       native_mcp_xaa_callback_port(),
       native_mcp_xaa_idp_client_secret());
   ```
   The seam returns `{nullopt, nullopt}` for a non-XAA server and
   `{port, secret}` otherwise. It is a pure function (no loader call, no
   filesystem), so the gate is hermetically testable exactly like
   `xaa_login_callback_port_for` (see test 7).

6. **Flow parameter** — `perform_mcp_oauth_flow()` gains
   `std::optional<std::string> xaa_idp_client_secret = std::nullopt`
   (sibling of `xaa_callback_port` at `auth.cppm:770`). After
   `get_xaa_config()` returns and before `authenticate_xaa()` is called:
   ```cpp
   if (xaa_idp_client_secret && !xaa_idp_client_secret->empty()) {
       xaa_config->idp_client_secret = *xaa_idp_client_secret;
   }
   ```
   `xaa_config` is a local `std::optional<XaaConfig>` (`auth.cppm:794`), so
   mutating it before passing `*xaa_config` to `authenticate_xaa()` is safe.
   This is the **injection point**: the secret enters the `XaaConfig` here,
   not from the file. Both `perform_mcp_oauth_flow()` call sites in
   `mcp_tool.cppm` must forward the new parameter — the `wait_for_callback`
   path (`mcp_tool.cppm:1635`) and the detached-thread path
   (`mcp_tool.cppm:1670`) — exactly as both already forward `xaa_port`.

7. **`authenticate_xaa()`** — unchanged. It already reads
   `config.idp_client_secret` (`xaa.cppm:906`) and passes it to
   `perform_cross_app_access()` (`xaa.cppm:711-712`). The field is populated
   by the injection above instead of by `read_xaa_config_file()`.

### Why the seam (not a direct services-layer read)

`perform_mcp_oauth_flow()` is in `cc.services.mcp.auth` (rank 7), and the
hardened store is in `cc.services.mcp.xaa_idp_login` (also rank 7). A direct
call (`get_idp_client_secret(xaa_config->idp_issuer)`) would compile and
work. The seam is preferred for three reasons:

1. **Symmetry with the port.** c17a established that the `--xaa` runtime
   path's XAA settings come from the composition layer, not from a
   services-layer file read. The secret should follow the same pattern so
   the two are reasoned about together.
2. **Issuer authority.** The issuer on the `--xaa` runtime path currently
   comes from the dead `xaa-idp.txt` file (`config.idp_issuer`). The
   authoritative issuer is `settings.xaaIdp.issuer`. The loader reads the
   secret under the *settings* issuer, which is the issuer the supported
   CLI wrote it under. A services-layer read would key off the dead file's
   issuer, reintroducing the two-store mismatch for the key.
3. **Testability.** The seam is hermetically testable with a fake loader
   (no filesystem, no ConfigManager), exactly as the c17a port seam is
   tested (`test_tools.cpp:9974-9996`).

## (c) File-config contract change

### What happens to `xaa-idp.txt` parsing

**Delete the `idp_client_secret=` parse.** The line is removed from
`read_xaa_config_file()` (`xaa.cppm:802-803`), exactly as c17a removed the
`callback_port=` line (`xaa.cppm:809-817` documents that removal). A
hand-edited `idp_client_secret=` line left in the file is ignored, exactly
like any other unknown key — the parser's `else` branch already skips
unrecognized keys silently (`xaa.cppm:808` falls through to the comment at
line 809).

**Why delete (not tolerate-and-warn, not silent fallback):**

- **Tolerate-and-warn** would keep the dead store as a fallback, preserving
  the asymmetry. The whole point is that the hardened store is the single
  source. A warning also risks leaking the secret into stderr if the warning
  includes the line content.
- **Silent fallback** (file value used when the store has none) is the
  current behavior — the asymmetry itself.
- **Delete** matches the c17a precedent for `callback_port` and the
  codebase convention: "prefer deleting dead code to fixing it" (CLAUDE.md).
  The `XaaConfig::idp_client_secret` field is **kept** (unlike
  `callback_port`, which was removed from the struct) because it is consumed
  in two places (C1, C2) and is now populated from the store via the seam.

The rest of `xaa-idp.txt` (issuer, client_id, client_secret, endpoints,
scope, idp_id_token) is **unchanged** by this design. Those fields are also
dead-store-sourced, but migrating them is a larger change (they have no
hardened-store counterpart and would need settings or store extensions) and
is out of scope for this deferred item.

### What happens to the `--xaa` CLI flag surface

**Unchanged.** The flag surface is:

| Flag | Where | Effect |
|---|---|---|
| `--xaa` on `/mcp add` | `mcp_cmd.cppm:610-611` | Marks server as XAA (`oauth.xaa = true`) |
| `--client-secret` on `/mcp add` | `mcp_cmd.cppm:600-601` | Reads `MCP_CLIENT_SECRET` env; for XAA servers stores under IdP issuer key (`mcp_cmd.cppm:685-694`) |
| `--client-secret` on `/mcp xaa setup` | `mcp_cmd.cppm:996-997` | Reads `MCP_XAA_IDP_CLIENT_SECRET` env; stores via `save_idp_client_secret()` (`mcp_cmd.cppm:1060-1063`) |

All three already write to the hardened store. No flag is added, removed, or
reinterpreted. The `--xaa` flag on `/mcp add` still requires
`LOOM_ENABLE_XAA=1`, `--client-id`, `--client-secret`, and a configured
`settings.xaaIdp` (`mcp_cmd.cppm:639-661`) — that validation is unchanged.

The file-config contract change is internal: `idp_client_secret=` in
`xaa-idp.txt` stops being a recognized key. No user-facing CLI change.

## (d) Compat / migration story

### Installs with the secret only in `xaa-idp.txt`

Because nothing in `src/` writes `xaa-idp.txt`, the only way to have an
`idp_client_secret=` line is to hand-edit the file. This is rare (the c17a
entry calls it "a dead surface a user could only reach by hand-editing"),
but the task requires a compat story.

**Migration: one-time read-once, in the loader.** The loader
(`install_core_settings_mcp_loader()`) is the natural place: it already
reads `settings().xaa_idp.issuer` and calls `get_idp_client_secret()`. The
migration extends that:

```
layer.xaa_idp_client_secret =
    get_idp_client_secret(settings().xaa_idp.issuer);
// One-time migration: check the legacy hand-edited file. GUARDED on a
// non-empty settings issuer: with an empty issuer (XAA not configured, or
// after `/mcp xaa clear` reset settings.xaaIdp to {}) save_idp_client_secret
// would write under the phantom key mcpXaaIdpConfig."".clientSecret, after
// which get_idp_client_secret("") no longer misses and the secret resurfaces.
if (!layer.xaa_idp_client_secret &&
    !settings().xaa_idp.issuer.empty()) {
    auto legacy = read_legacy_idp_client_secret();
    if (legacy) {
        save_idp_client_secret(settings().xaa_idp.issuer, *legacy);
        layer.xaa_idp_client_secret = *legacy;
    }
}
```

`read_legacy_idp_client_secret()` is a new function in
`cc.services.mcp.xaa_idp_login` (the store module, so the loader only needs
the one services import it already gains). It reads **only** the
`idp_client_secret=` line from `~/.loom/xaa-idp.txt`, using the hardened
`read_regular_file()` (`atomic_replace.cppm:287`) — not `std::ifstream` — so
the migration read is symlink-safe and FIFO-safe (see security notes).

**Why read-once (not permanent fallback):**

- The migration fires on the **first loader call** that sees a non-empty
  settings issuer and no stored secret — not specifically on a `--xaa` auth.
  The loader runs inside `NativeMcpRuntime::ensure_loaded_from_config()`
  (`mcp_tool.cppm:974-978`), which ordinary config loads also hit
  (`all_statuses()` → `/mcp list`, startup auto-connect, `status()`,
  `restart()`, `call_tool()`, …), so ANY user with a hand-edited
  `idp_client_secret=` line and a configured `settings.xaaIdp.issuer` —
  including non-XAA users — migrates on the first such load.
  (`reload_from_config()` (`mcp_tool.cppm:1009-1019`) resets `loaded_` and
  re-runs the loader, so the once-per-process guarantee rests on
  self-disabling, not on the flag: a re-run finds the store populated and
  skips the legacy-read branch.)
  The behavior is harmless: it is self-disabling (the store now has the
  secret, so the legacy-read branch is never reached again) and moves the
  secret into the 0600 store. After that first call, the secret is in the
  hardened store. Every subsequent loader call finds it there.
- The legacy file is never modified or deleted by the migration — the line
  is simply no longer read after the store has the secret. This avoids
  mutating a hand-edited file and avoids any write-permission surprise.
- If the hardened store's secret is later cleared (`/mcp xaa clear`, W8),
  the migration does **not** re-fire. `execute_xaa_clear()`
  (`mcp_cmd.cppm:1154-1168`) removes the secret under the old issuer AND
  resets `settings.xaaIdp` to `{}` (issuer becomes empty); the
  non-empty-issuer guard above then suppresses the legacy read entirely, so
  the hand-edited line stays dormant. (Before the guard was specified this
  was a live bug: the next loader call would re-read the legacy line, store
  it under the empty key `mcpXaaIdpConfig."".clientSecret`, set
  `layer.xaa_idp_client_secret`, and the secret would resurface on the next
  XAA auth — silently undoing the clear.) The only way the legacy line can
  re-migrate after a clear is if the user re-runs
  `/mcp xaa setup --issuer <X>` (issuer non-empty again) with the store
  still empty — acceptable, since the file is still a valid hand-edited
  source. No `migratedFromLegacy` marker is needed.

**Why in the loader (not in `perform_mcp_oauth_flow`):**

- The loader is the composition root that owns the settings issuer. The
  migration keys off that issuer, not the dead file's issuer.
- `perform_mcp_oauth_flow()` stays a pure consumer of injected values — it
  does not gain a filesystem read for a legacy format.
- The migration is testable hermetically through the loader seam (fake
  loader + real store with isolated HOME), without running the XAA network
  flow.

### Installs with the secret in the hardened store (the normal case)

No migration. `get_idp_client_secret()` returns the stored value, the loader
injects it through the seam, and `perform_mcp_oauth_flow()` sets it on
`XaaConfig`. The legacy file is never read.

### Installs with no secret anywhere

`idp_client_secret` stays `nullopt`. The XAA flow degrades to PKCE-only at
the IdP (the `if (opts.idp_client_secret && ...)` guards at
`xaa_idp_login.cppm:910` and `xaa.cppm:474` skip the secret). This is the
existing behavior for a public IdP client and is unchanged.

## (e) Security notes

### File modes

- **Hardened store**: 0600 file / 0700 directory, enforced by
  `atomic_replace_file(..., AtomicMode::OwnerOnly)` (c18;
  `xaa_idp_login.cppm:261,284,320,366`). The directory is forced to 0700 by
  `ensure_owner_only_store_dir()` (`xaa_idp_login.cppm:164-173`). The secret
  is owner-only at rest.
- **Legacy `xaa-idp.txt`**: no mode guarantees. `read_xaa_config_file()`
  opens it with `std::ifstream` (`xaa.cppm:767`), which follows symlinks and
  applies the umask on creation (but nothing in `src/` creates it). A
  hand-edited file is typically 0644. After migration, the secret lives in
  the 0600 store; the legacy file's wider mode is a pre-existing condition
  this design does not worsen (the file was already read for the other
  fields before this change).

### Presence-only reads

- `read_xaa_idp_status()` (`mcp_cmd.cppm:440-441`) already does
  presence-only: `has_client_secret` is a bool, the value is never returned
  to the UI. `/mcp xaa show` prints "(stored in keychain)" or "(not set —
  PKCE-only)" (`mcp_cmd.cppm:1144-1145`), never the value.
- The composition seam carries the **actual secret value** (it must — the
  IdP token exchange needs it), but it transits only through:
  `CoreSettingsMcpLayer` (in-memory struct) → `NativeMcpRuntime` accessor →
  `perform_mcp_oauth_flow` parameter → `XaaConfig` field → HTTP form body.
  It is never logged, printed, or returned in a tool result.

### No secret bytes in logs / errors / stderr

- `redact_tokens()` (`xaa.cppm:189-196`) redacts `client_secret` (and the
  other token fields) from HTTP response bodies before they appear in error
  messages. It is applied on the **C2** RFC 8693 token-exchange leg
  (`xaa.cppm:485-486, 509, 580-581`). The **C1** OIDC login leg's
  token-exchange error path (`xaa_idp_login.cppm:944-948`) currently returns
  `token_response->body.substr(0, 200)` **unredacted** — this design must add
  the same one-line wrap there:
  ```cpp
  + detail::redact_tokens(
        std::string_view(token_response->body).substr(0, 200))
  ```
  This matters more after this design ships: every `/mcp xaa setup
  --client-secret` user now has the secret on the C1 leg (via
  `login_opts.idp_client_secret` at `xaa.cppm:906`), not just hand-editors,
  and an IdP that echoes `client_secret` in a 400 body would otherwise leak
  it into the `mcp_auth` tool result.
- **Redaction is JSON-only.** `redact_tokens()`'s regex matches
  `"client_secret":"..."` (JSON shape); a form-encoded or plain-text error
  body (`client_secret=...&error=...`) is NOT redacted. IdP error bodies are
  usually JSON (RFC 6749 §5.2 defines a JSON error object) but not
  guaranteed — a non-JSON body that echoes the secret would still leak. This
  is a pre-existing limitation of the shared helper, not something this
  design introduces; noted so the C1 wrap above is not mistaken for full
  coverage.
- The migration must not log the secret value. A debug trace may record
  "XAA: migrated IdP client secret from legacy xaa-idp.txt to hardened
  store" — presence-only, no bytes.
- The `XaaTokenExchangeError` and `Error` messages in the XAA flow include
  URLs and HTTP statuses but never the secret — **provided** the response
  body is redacted as above. The secret goes into the request body, but an
  IdP MAY echo request parameters in its error response (some do, for
  debugging), which is why both legs wrap the echoed body in
  `redact_tokens()` before it reaches the error message.
- `perform_mcp_oauth_flow()`'s error path (`auth.cppm:812-814`) wraps
  `xaa_result.error().message()`, which is already redacted.

### Symlink / FIFO hazards on the store path

- **Hardened store writes**: `atomic_replace_file()` refuses a symlink,
  FIFO, socket, or device leaf (`atomic_replace.cppm:112-121`), opens the
  tmp with `O_EXCL|O_NOFOLLOW` (`atomic_replace.cppm:151-154`), and fsyncs
  before rename. The store path is safe.
- **Hardened store reads**: `read_idp_client_secret()` currently uses
  `cc::utils::json::parse_file()` (`xaa_idp_login.cppm:329`), which is a
  path-based read. This is a **pre-existing** read-side TOCTOU (a same-uid
  actor could swap a FIFO over the leaf between the `exists` check and the
  open). The c18 hardening closed the write side; the read side uses
  `parse_file` which does not have the `O_NOFOLLOW|O_NONBLOCK` gate that
  `read_regular_file()` has. This design does not change the read path (the
  secret is read through the existing `get_idp_client_secret()`), but the
  migration read **must** use `read_regular_file()` to avoid introducing a
  new unhardened read. A follow-up should route `read_idp_client_secret()`
  and `read_cached_idp_token()` through `read_regular_file()` too — recorded
  as an open item below.
- **Legacy file read (migration)**: `read_legacy_idp_client_secret()` uses
  `read_regular_file()` (`atomic_replace.cppm:287`), which opens with
  `O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC`, fstat-gates on `S_ISREG`, and
  maps `ENOENT` to absent. A symlinked or FIFO `xaa-idp.txt` yields
  "absent" (no block, no follow), so the migration is safe against a
  malicious leaf.

## (f) Test plan

All tests are hermetic (no network, no browser, no real IdP). They use the
existing `C17IdpTokenCacheGuard` (`test_services.cpp:12583`) and
`c13_make_temp_root` (`test_services.cpp:150`) helpers for isolated HOME, and
`CoreSettingsMcpLoaderGuard` for the loader seam.

### 1. Seam forwarding (mirrors c17a port test, `test_tools.cpp:9974`)

- **`CoreSettingsLoaderCarriesXaaIdpClientSecret`**: install a fake loader
  that yields `layer.xaa_idp_client_secret = "test-secret"`; assert
  `native_mcp_xaa_idp_client_secret()` returns `"test-secret"`. Replace with
  a loader that yields no secret; assert `nullopt`. (Same shape as
  `CoreSettingsLoaderCarriesXaaCallbackPort`.)

### 2. File-config contract (mirrors c17a callback_port test,
`test_services.cpp:12771`)

- **`XaaIdpFileClientSecretLineIsIgnored`**: write an `xaa-idp.txt` with
  `idp_client_secret=legacy-secret` (and the minimum required fields); call
  `get_xaa_config("any")`; assert the returned `XaaConfig` has
  `idp_client_secret == nullopt`. A `static_assert` or concept check pins
  that the field still EXISTS on `XaaConfig` (the inverse of the c17a
  `!HasCallbackPort` check at `test_services.cpp:12789`) — it cannot pin
  that the field is populated from the store; population behavior is pinned
  by the `nullopt` assertion above (and by tests 1 and 8).

### 3. Round-trip (already exists, extended)

- **`SaveGetIdpClientSecretRoundTrip`**: `save_idp_client_secret(issuer,
  "rt-secret")` → `get_idp_client_secret(issuer)` returns `"rt-secret"`.
  Assert the store file mode is 0600 and the directory is 0700 (the c18
  mode test, extended to cover the secret entry).

### 4. Presence-only (already exists, extended)

- **`ReadXaaIdpStatusPresenceOnly`**: store a secret; call
  `read_xaa_idp_status()`; assert `has_client_secret == true` and the
  status struct has no field carrying the value. `/mcp xaa show` output
  contains "(stored in keychain)" and does not contain the secret bytes.

### 5. No-leak

- **`NoSecretBytesInErrorsOrLogs`**: drive the `--xaa` path with a stored
  secret against a mock IdP that returns a 400 whose body contains the
  secret bytes; assert the resulting error message does not contain the
  secret bytes. The test must drive BOTH legs, or at minimum the C1 leg:
  there is no existing `redact_tokens()` test in `test_services.cpp` (grep
  confirms zero), and the fixture this plan mirrors
  (`test_services.cpp:5109`, `PerformsXaaIdpLoginAndStoresTokens`) sets
  `idp_id_token=`, which skips C1 entirely (the `if (id_token.empty())`
  branch at `xaa.cppm:896` is not taken). To drive C1, leave `idp_id_token`
  EMPTY in `xaa-idp.txt` (and no cached token) so
  `acquire_idp_id_token()` runs, with the mock IdP's token endpoint
  returning the 400. Drive C2 separately (or in the same case) by having the
  RFC 8693 exchange return the 400. Also assert the migration trace (if any)
  contains no secret bytes.

### 6. Old-file migration

- **`LegacyXaaIdpFileSecretMigratedToStore`**: with an isolated HOME, write
  `idp_client_secret=legacy-only-secret` to `~/.loom/xaa-idp.txt` (plus
  minimum required fields); leave the hardened store absent. Install the
  real loader (or call the migration function directly); assert:
  (a) `get_idp_client_secret(issuer)` now returns `"legacy-only-secret"`
  (migrated to the hardened store), (b) the store file is 0600, (c) the
  legacy file is not re-read on a second call — pinned observably by
  DELETING (or renaming) `~/.loom/xaa-idp.txt` after the first load and
  asserting the second loader call still yields the secret (a re-read would
  fail on the missing file), or by asserting the store value's presence
  directly.

- **`LegacyXaaIdpFileSymlinkDoesNotBlock`**: replace `xaa-idp.txt` with a
  symlink to `/dev/null` (or a FIFO); call the migration; assert it returns
  `nullopt` without blocking (the `read_regular_file()` gate).

### 7. Gate (mirrors c17a is_xaa gate test)

- **`XaaSecretForwardedOnlyForXaaServer`**: a loader that yields a secret;
  `mcp_auth` on a non-XAA OAuth server must not surface the secret (the
  `is_xaa` gate). Hermetic: assert the `perform_mcp_oauth_flow` call receives
  `nullopt` for the secret parameter on a non-XAA server. The gate is the
  pure seam `xaa_login_secrets_for(is_xaa, port, secret)` — committed here as
  the sibling of `xaa_login_callback_port_for` (`mcp_tool.cppm:1374-1378`)
  rather than left as an inline ternary, so this test is as hermetic as the
  port's `xaa_login_callback_port_for` test (no flow, no loader, no
  filesystem).

### 8. End-to-end on the `--xaa` path (mirrors
`test_services.cpp:5109`, extended)

- **`XaaRuntimePathUsesHardenedStoreSecret`**: with a mock IdP/AS (the
  existing `LocalXaaIdpServer` fixture), store a secret via
  `save_idp_client_secret()`; do NOT put `idp_client_secret=` in
  `xaa-idp.txt`; run `perform_mcp_oauth_flow()` with the secret injected;
  assert the IdP token-exchange request received `client_secret=<stored
  value>` (the mock server records the form body). This is the regression
  test that would have caught the original asymmetry.

## Open questions / deferred items

1. **Read-side hardening of the hardened store.** `read_idp_client_secret()`
   and `read_cached_idp_token()` use `json::parse_file()` (path-based, no
   `O_NOFOLLOW` gate). The c18 hardening closed the write side; the read
   side should route through `read_regular_file()` for the same
   symlink/FIFO protection. Pre-existing, out of scope here.

2. **The rest of `XaaConfig` still comes from the dead file.** `issuer`,
   `client_id`, `client_secret`, `idp_token_endpoint`, `scope`, `idp_id_token`
   are still read from `xaa-idp.txt` by `read_xaa_config_file()`. Migrating
   these to settings + the hardened store is a larger change (some have no
   store counterpart) and is a separate follow-up. This design closes the
   secret asymmetry only, as scoped by the deferred item.

3. **`/mcp add --xaa --client-secret` stores the AS secret under the IdP
   issuer key.** `mcp_cmd.cppm:691-692` stores `MCP_CLIENT_SECRET` (the AS
   client secret) via `save_idp_client_secret(xaa_cfg.issuer, ...)`, which is
   the IdP secret slot. This may be intentional (same secret for both AS and
   IdP in many setups) or a conflation. It is pre-existing and unchanged by
   this design; recorded for a future review.

4. **Re-migration after `xaa clear`.** Resolved by the non-empty-issuer
   guard (section (d)): `/mcp xaa clear` resets `settings.xaaIdp` to `{}`,
   so the migration cannot re-fire after a clear — the legacy line stays
   dormant. (An earlier draft dismissed this question on the premise that a
   re-migrated secret lands under a usable issuer key; it lands under the
   empty key `""`, which is exactly why the guard is required — both reviews
   independently flagged this.) The only re-migration path after a clear is
   a fresh `/mcp xaa setup --issuer <X>` with the store still empty, which
   is acceptable. No `migratedFromLegacy` marker needed.

## Review history

Two adversarial design reviews on 2026-09-29, both verdict request-changes.
All required changes applied in this revision: (1) guard the legacy migration
on a non-empty settings issuer (empty-issuer `save_idp_client_secret("", ...)`
pollution and `/mcp xaa clear` resurrection — flagged independently by both
reviews); (2) state the actual migration trigger (first loader call via
`ensure_loaded_from_config()`, not the first `--xaa` auth); (3) reword the
test-2 `static_assert` claim to pin field existence, not store population;
(4) add `redact_tokens()` to the C1 IdP-login token-exchange error path
(`xaa_idp_login.cppm:944-948`) and correct the security section to name both
legs; (5) pin test 5 to drive the C1 leg (empty `idp_id_token`, mock IdP 400
echoing the secret); (6) same empty-issuer guard as (1). Also folded in the
non-blocking notes: JSON-only redaction limitation, the pure
`xaa_login_secrets_for(is_xaa, port, secret)` seam, both
`perform_mcp_oauth_flow` call sites forwarding the secret, and an observable
pin for test 6(c).

Independent design verification (agent:design-verify, 2026-09-29): **approved**
— every required change confirmed against the live tree. One minor
parenthetical correction applied in the same pass: the once-per-process
migration guarantee rests on self-disabling, not on the `loaded_` flag (which
`reload_from_config()` resets).
