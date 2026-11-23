"""RFC 0040 shared closed visual tables and the structural classifier.

One module for the four journey-family generators (timeline, gantt, journey and
the RFC 0040 sibling). The hermetic ctests run `python3 -I`, which implies `-P`,
so a plain sibling import fails; load this file with
`importlib.util.spec_from_file_location` and an explicit path (the pattern the
existing admission verifiers use).

The classifier mirrors the three TU-local C++ validators: a `theme`/`look` key
is a visual key only when the line contains a colon and its key (text before
the first colon, leading spaces stripped) is exactly `theme` or `look`; it is
admitted only at indent 2 inside the `config:` block, with a value inside the
family's table and at most one occurrence per key. A visual key anywhere else
keeps the whole source, and an in-body `%%{...theme...}%%` initializer does too.
Every other frontmatter line is ignored (the pass-through contract).
"""

# Closed corpus-bounded value tables (RFC 0040 section 1).
LOOK_TABLE = {
    "timeline": ("classic", "neo"),
    "gantt": (),
    "journey": (),
}
THEME_TABLE = {
    "timeline": (
        "dark", "default", "forest", "neo", "neo-dark", "neutral",
        "redux", "redux-dark", "redux-color", "redux-dark-color",
    ),
    "gantt": (
        "dark", "default", "forest", "neutral", "redux", "redux-dark",
        "redux-color", "redux-dark-color",
    ),
    "journey": ("redux", "redux-dark", "redux-color", "redux-dark-color"),
}
NOTICE_HEADER = {
    "timeline": "Timeline native projection v1",
    "gantt": "Gantt native projection v1",
    "journey": "Journey native projection v1",
}

# RFC 0047: closed leading-directive admission. Gantt keeps the legacy
# "%%{ line mentioning theme" rule only, so its table constant is empty and
# never consulted (the mirror returns it for shape parity).
INIT_ENABLED = {"timeline": True, "gantt": False, "journey": True}
INIT_THEME_TABLE = {
    "timeline": ("base", "dark", "default", "forest", "neutral"),
    "gantt": (),
    "journey": ("dark", "default", "forest", "neutral"),
}
LOG_LEVELS = ("0", "1", "2", "3", "4", "5",
              "trace", "debug", "info", "warn", "error", "fatal")

# D-454: the closed nested directive objects. Only the timeline family admits
# them; journey and gantt keep the RFC 0047 flat grammar. A leaf is
# (outer, inner-or-empty-for-cScaleN, kind) with kind in {"bool", "colour"}.
NESTED_TABLE = {
    "timeline": (("timeline", "disableMulticolor", "bool"),
                 ("themeVariables", "", "colour")),
    "journey": (),
    "gantt": (),
}


def sanitize_breaks(text):
    """Mirror the gantt renderer's `<br ...>` sanitizing (each tag -> one space)."""
    out = []
    i = 0
    size = len(text)
    while i < size:
        matched = 0
        if text[i] == "<" and i + 2 < size and text[i + 1].lower() == "b" and text[i + 2].lower() == "r":
            j = i + 3
            while j < size and text[j] in " \t":
                j += 1
            if j < size and text[j] == "/":
                j += 1
            while j < size and text[j] in " \t":
                j += 1
            if j < size and text[j] == ">":
                matched = j - i + 1
        if matched:
            out.append(" ")
            i += matched
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def split_frontmatter(text):
    """Return (frontmatter_lines or None, body).

    Fence matching is exact, mirroring the C++ gates (`starts_with("---\\n")`,
    `find("\\n---\\n")`): a fence with trailing whitespace or a CR is not a
    frontmatter envelope for the renderer, so it must not be one here either.
    """
    lines = text.split("\n")
    if not lines or lines[0] != "---":
        return None, text
    for close in range(1, len(lines)):
        if lines[close] == "---":
            return lines[1:close], "\n".join(lines[close + 1:])
    return None, text


def scalar(raw):
    """Trim both sides, strip a trailing comment outside quotes, dequote a match."""
    value = raw.strip(" \t")
    quote = ""
    cut = len(value)
    for index, char in enumerate(value):
        if quote:
            if char == quote:
                quote = ""
            continue
        if char in "'\"":
            quote = char
        elif char == "#" and (index == 0 or value[index - 1] in " \t"):
            cut = index
            break
    value = value[:cut].strip(" \t")
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "'\"":
        value = value[1:-1]
    return value


def _quotes_ok(text):
    """Mirror normalize_directive_quotes: balanced quotes, no control bytes,
    no backslash inside a single-quoted string."""
    at, size = 0, len(text)
    while at < size:
        quote = text[at]
        if quote not in "'\"":
            if ord(quote) < 0x20:
                return False
            at += 1
            continue
        at += 1
        escape = False
        while at < size:
            char = text[at]
            if quote == "'" and char == "\\":
                return False
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                break
            elif ord(char) < 0x20:
                return False
            at += 1
        if at == size:
            return False
        at += 1
    return True


def _split_outside_quotes(text, sep):
    parts, quote, start = [], "", 0
    at = 0
    while at < len(text):
        char = text[at]
        if quote:
            if char == quote:
                quote = ""
        elif char in "'\"":
            quote = char
        elif char == sep:
            parts.append(text[start:at])
            start = at + 1
        at += 1
    parts.append(text[start:])
    return parts


def _first_colon(text):
    quote, escape = "", False
    for at, char in enumerate(text):
        if quote:
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = ""
            continue
        if char in "'\"":
            quote = char
        elif char == ":":
            return at
    return -1


def _split_entries(text):
    """Top-level comma split; commas inside `{...}` or quotes do not split."""
    parts, quote, depth, start = [], "", 0, 0
    for at, char in enumerate(text):
        if quote:
            if char == quote:
                quote = ""
        elif char in "'\"":
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth < 0:
                return None
        elif char == "," and depth == 0:
            parts.append(text[start:at])
            start = at + 1
    if quote or depth != 0:
        return None
    parts.append(text[start:])
    return parts


def _boolean(value):
    return value in ("true", "false")


def _colour(value):
    if len(value) not in (4, 7) or not value.startswith("#"):
        return False
    return all(char in "0123456789abcdefABCDEF" for char in value[1:])


def _cscale(key):
    if not key.startswith("cScale"):
        return False
    digits = key[len("cScale"):]
    return (digits.isascii() and digits.isdigit() and
            1 <= len(digits) <= 2 and int(digits) <= 11 and
            (digits == "0" or digits[0] != "0"))


def _nested_object(outer, object_text, nested, leaves, seen, idents):
    """Mirror journey_fam_nested_object; appends disclosure lines to `leaves`."""
    if len(object_text) < 2 or object_text[0] != "{" or object_text[-1] != "}":
        return False
    interior = object_text[1:-1]
    if any(char in interior for char in "{}[]"):
        return False
    if not _quotes_ok(interior):
        return False
    entries = _split_entries(interior)
    if not entries:
        return False
    for entry in entries:
        colon = _first_colon(entry)
        if colon < 0:
            return False
        key = scalar(entry[:colon])
        value = scalar(entry[colon + 1:])
        leaf = None
        for candidate in nested:
            if candidate[0] != outer:
                continue
            if candidate[1] == key or (candidate[1] == "" and _cscale(key)):
                leaf = candidate
        if leaf is None:
            return False
        if leaf[2] == "bool":
            if not _boolean(value):
                return False
        elif not _colour(value):
            return False
        ident = outer + "." + key
        if ident in seen:
            return False
        seen.add(ident)
        idents.append(ident)
        leaves.append(f"config.{outer}.{key}={value} -> colour palette; not applied")
    return True


def parse_init_directive(line, theses, nested=()):
    """Return (admitted, log_level, theme, nested_lines) for one directive.

    `theses` is the family's closed directive theme table; `nested` is the
    D-454 closed nested-object table. A duplicated key inside one payload is
    rejected here; duplicates across directives are the caller's job.
    """
    text = line.rstrip(" \t")
    if not text.startswith("%%{") or not text.endswith("}%%") or len(text) < 7:
        return False, None, None, [], []
    directive = text[3:-3].strip(" \t")
    if directive == "init":
        return True, None, None, [], []
    if not directive.startswith("init:"):
        return False, None, None, [], []
    payload = directive[5:].strip(" \t")
    if len(payload) < 2 or payload[0] != "{" or payload[-1] != "}":
        return False, None, None, [], []
    interior = payload[1:-1]
    if any(char in interior for char in "[]"):
        return False, None, None, [], []
    if not _quotes_ok(interior):
        return False, None, None, [], []
    if not interior.strip(" \t"):
        return True, None, None, [], []
    entries = _split_entries(interior)
    if not entries:
        return False, None, None, [], []
    log_level = theme = None
    nested_lines, seen, nested_idents = [], set(), []
    for entry in entries:
        colon = _first_colon(entry)
        if colon < 0:
            return False, None, None, [], []
        key = scalar(entry[:colon])
        # Mirror journey_fam_rtrim(journey_fam_ltrim(...)): left trims spaces
        # only, right trims spaces and tabs.
        raw = entry[colon + 1:].lstrip(" ").rstrip(" \t")
        if raw.startswith("{"):
            if not _nested_object(key, raw, nested, nested_lines, seen, nested_idents):
                return False, None, None, [], []
            continue
        value = scalar(raw)
        if key == "logLevel":
            if log_level is not None or value not in LOG_LEVELS:
                return False, None, None, [], []
            log_level = value
        elif key == "theme":
            if theme is not None or value not in theses:
                return False, None, None, [], []
            theme = value
        else:
            return False, None, None, [], []
    return True, log_level, theme, nested_lines, nested_idents


def classify_init(family, body):
    """Mirror the RFC 0047 leading-block scan (D-454 nested objects included).

    Returns (kind, init_leaves) where kind is None when the body is clean,
    "gate" when the block is rejected and "admit" when every leading directive
    is inside the closed grammar. `init_leaves` carries any admitted values;
    `init_nested` is a list of disclosure lines in source order.
    """
    if not INIT_ENABLED[family]:
        return None, {}
    lines = body.split("\n")
    leaves = {}
    index = 0
    while index < len(lines):
        line = lines[index]
        if line.endswith("\r"):
            line = line[:-1]
        line = line.lstrip(" ")
        if not line or (line.startswith("%%") and not line.startswith("%%{")):
            index += 1
            continue
        if not line.startswith("%%{"):
            break
        # D-454: join the following lines until the closing `}%%`, bounding the
        # prospective size exactly as the renderer does.
        buffer = line
        while not buffer.rstrip(" \t").endswith("}%%"):
            index += 1
            if index >= len(lines):
                return "gate", {}
            part = lines[index]
            if part.endswith("\r"):
                part = part[:-1]
            piece = part.lstrip(" ")
            if len(buffer) + 1 + len(piece) > 2048:
                return "gate", {}
            buffer += " " + piece
        ok, log_level, theme, nested, nested_idents = parse_init_directive(
            buffer, INIT_THEME_TABLE[family], NESTED_TABLE[family])
        if not ok:
            return "gate", {}
        if log_level is not None:
            if "init_logLevel" in leaves:
                return "gate", {}
            leaves["init_logLevel"] = log_level
        if theme is not None:
            if "init_theme" in leaves:
                return "gate", {}
            leaves["init_theme"] = theme
        if nested:
            # A leaf identity (`outer.key`) repeated across directives is a
            # duplicate and gates, matching the renderer's shared seen-set.
            seen = leaves.get("init_nested_idents", [])
            if any(ident in seen for ident in nested_idents):
                return "gate", {}
            leaves["init_nested_idents"] = seen + nested_idents
            leaves["init_nested"] = leaves.get("init_nested", []) + nested
        index += 1
    return "admit", leaves


def body_has_theme_init(body):
    """The legacy whole-body rule kept verbatim for the disabled families."""
    for line in body.split("\n"):
        stripped = line.lstrip(" \t")
        if stripped.startswith("%%{") and "theme" in line:
            return True
    return False


def tail_has_theme_init(body):
    """The legacy rule from the first non-leading body line onward."""
    lines = body.split("\n")
    started = False
    for line in lines:
        stripped = line.lstrip(" \t")
        if not stripped:
            continue
        if stripped.startswith("%%") and not stripped.startswith("%%{"):
            continue
        if not started:
            if not stripped.startswith("%%{"):
                started = True
            continue
        if stripped.startswith("%%{") and "theme" in line:
            return True
    return False


def classify(family, text):
    """Return (kind, leaves).

    kind is one of:
      "drawn"    -- no visual key and no body theme initializer;
      "admitted" -- exactly the visual keys admitted by RFC 0040 (indent 2 in
                    `config:`, in-table values, at most one per key);
      "themed"   -- any other visual key (top level, payload, indent >= 4,
                    out-of-table value) or a body theme initializer.
    leaves maps "look"/"theme" to the admitted scalar for "admitted" rows.
    """
    if family == "gantt":
        text = sanitize_breaks(text)
    frontmatter, body = split_frontmatter(text)
    leaves = {}
    if frontmatter is not None:
        in_config = False
        for raw in frontmatter:
            line = raw.rstrip("\r")
            indent = len(line) - len(line.lstrip(" "))
            content = line[indent:]
            if not content:
                continue
            column0 = indent == 0
            colon = content.find(":")
            if column0:
                if content.startswith("#"):
                    continue
                key = content if colon < 0 else content[:colon]
                if colon >= 0 and key in ("theme", "look"):
                    return "themed", {}
                in_config = colon >= 0 and key == "config"
                continue
            if colon < 0:
                continue
            key = content[:colon]
            if key not in ("theme", "look"):
                continue
            if not in_config or indent != 2:
                return "themed", {}
            value = scalar(content[colon + 1:])
            table = LOOK_TABLE[family] if key == "look" else THEME_TABLE[family]
            if key in leaves or value not in table:
                return "themed", {}
            leaves[key] = value
    if not INIT_ENABLED[family]:
        if body_has_theme_init(body):
            return "themed", {}
        return ("admitted", leaves) if leaves else ("drawn", {})
    init_kind, init_leaves = classify_init(family, body)
    if init_kind == "gate":
        return "themed", {}
    # G5: a frontmatter visual leaf and a keyed leading directive are
    # mutually exclusive channels.
    if leaves and init_leaves:
        return "themed", {}
    if tail_has_theme_init(body):
        return "themed", {}
    if init_leaves:
        return "init_admitted", init_leaves
    return ("admitted", leaves) if leaves else ("drawn", {})


def notice_lines(family, leaves, init_leaves=None):
    """The literal disclosure footer the renderer appends below the art.

    `init_leaves` (RFC 0047) carries admitted directive values as
    `init_logLevel` / `init_theme`, emitted after the frontmatter lines in the
    canonical logLevel-before-theme order.
    """
    init_leaves = init_leaves or {}
    lines = []
    if "look" in leaves:
        lines.append(f"config.look={leaves['look']} -> source node style; not applied")
    if "theme" in leaves:
        lines.append(f"config.theme={leaves['theme']} -> colour palette; not applied")
    if "init_logLevel" in init_leaves:
        lines.append(
            f"config.logLevel={init_leaves['init_logLevel']} -> logger only; not applied")
    if "init_theme" in init_leaves:
        lines.append(
            f"config.theme={init_leaves['init_theme']} -> colour palette; not applied")
    for nested in init_leaves.get("init_nested", []):
        lines.append(nested)
    return [NOTICE_HEADER[family]] + lines if lines else []


def load(path):
    """Import this module from an explicit path (python3 -I implies -P)."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("journeyfam_tables", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
