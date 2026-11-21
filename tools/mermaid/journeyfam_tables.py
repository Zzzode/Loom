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


def body_has_theme_init(body):
    for line in body.split("\n"):
        stripped = line.lstrip(" \t")
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
    if body_has_theme_init(body):
        return "themed", {}
    return ("admitted", leaves) if leaves else ("drawn", {})


def notice_lines(family, leaves):
    """The literal disclosure footer the renderer appends below the art."""
    lines = []
    if "look" in leaves:
        lines.append(f"config.look={leaves['look']} -> source node style; not applied")
    if "theme" in leaves:
        lines.append(f"config.theme={leaves['theme']} -> colour palette; not applied")
    return [NOTICE_HEADER[family]] + lines if lines else []


def load(path):
    """Import this module from an explicit path (python3 -I implies -P)."""
    import importlib.util
    spec = importlib.util.spec_from_file_location("journeyfam_tables", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
