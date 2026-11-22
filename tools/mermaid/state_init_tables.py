"""Python mirror of the RFC 0048 P2 state-local leading-directive grammar.

This module is the renderer-free side of the single-source probe artifact
`tools/mermaid/state-init-0048-probes.json` (efficiency rule 37): the C++ gate
in tests/ui/test_mermaid_state_region_0048_cohort.cpp reads the same artifact
and both sides must agree on every case.

The grammar is a closed copy of `state_init_directive` in
src/ui/visual/mermaid_relations_core_impl.cpp: one flat object over
`logLevel`/`theme`, quote state machine, no nesting, no duplicate key.
"""

LOG_LEVELS = ("0", "1", "2", "3", "4", "5", "trace", "debug", "info", "warn", "error",
              "fatal")
THEMES = ("dark", "default", "forest", "neutral")


def _rtrim(text):
    return text.rstrip(" \t")


def _ltrim(text):
    return text.lstrip(" \t")


def _quotes_ok(text):
    at = 0
    while at < len(text):
        quote = text[at]
        if quote not in "'\"":
            if ord(quote) < 0x20:
                return False
            at += 1
            continue
        at += 1
        escape = False
        while at < len(text):
            c = text[at]
            if quote == "'" and c == "\\":
                return False
            if escape:
                escape = False
                at += 1
                continue
            if c == "\\":
                escape = True
                at += 1
                continue
            if c == quote:
                break
            if ord(c) < 0x20:
                return False
            at += 1
        if at == len(text):
            return False
        at += 1
    return True


def _split_commas(text):
    parts, quote, start = [], None, 0
    for at, c in enumerate(text):
        if quote:
            if c == quote:
                quote = None
            continue
        if c in "'\"":
            quote = c
            continue
        if c == ",":
            parts.append(text[start:at])
            start = at + 1
    parts.append(text[start:])
    return parts


def _first_colon(text):
    quote, escape = None, False
    for at, c in enumerate(text):
        if quote:
            if escape:
                escape = False
                continue
            if c == "\\":
                escape = True
                continue
            if c == quote:
                quote = None
            continue
        if c in "'\"":
            quote = c
            continue
        if c == ":":
            return at
    return -1


def _scalar(raw):
    value = _rtrim(_ltrim(raw))
    quote = None
    for n, c in enumerate(value):
        if quote:
            if c == quote:
                quote = None
            continue
        if c in "'\"":
            quote = c
            continue
        if c == "#" and (n == 0 or value[n - 1] in " \t"):
            value = _rtrim(_ltrim(value[:n]))
            break
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "'\"":
        value = value[1:-1]
    return value


def parse_directive(line):
    """Return (log_level, theme) for one admitted directive line, else None.

    The bare `init` no-op and an empty `{}` return (None, None).
    """
    line = _rtrim(line)
    if not line.startswith("%%{") or not line.endswith("}%%") or len(line) < 7:
        return None
    inner = line[3:-3]
    directive = _rtrim(_ltrim(inner))
    if directive == "init":
        return (None, None)
    if not directive.startswith("init:"):
        return None
    payload = _rtrim(_ltrim(directive[5:]))
    if len(payload) < 2 or payload[0] != "{" or payload[-1] != "}":
        return None
    interior = payload[1:-1]
    if any(c in interior for c in "{}[]"):
        return None
    if not _quotes_ok(interior):
        return None
    if not _rtrim(_ltrim(interior)):
        return (None, None)
    log_level, theme = None, None
    for entry in _split_commas(interior):
        colon = _first_colon(entry)
        if colon < 0:
            return None
        key, value = _scalar(entry[:colon]), _scalar(entry[colon + 1:])
        if key == "logLevel":
            if log_level is not None or value not in LOG_LEVELS:
                return None
            log_level = value
        elif key == "theme":
            if theme is not None or value not in THEMES:
                return None
            theme = value
        else:
            return None
    return (log_level, theme)


def is_leading(source_body_lines):
    """True when the first non-blank, non-comment line starts a directive."""
    for line in source_body_lines:
        stripped = _ltrim(line.rstrip("\r"))
        if not stripped:
            continue
        if stripped.startswith("%%") and not stripped.startswith("%%{"):
            continue
        return stripped.startswith("%%{")
    return False


def footer_lines(log_level, theme):
    """The directive part of the disclosure footer, in the fixed leaf order."""
    lines = ["State native projection v1"]
    if log_level is not None:
        lines.append(f"config.logLevel={log_level} -> logger only; not applied")
    if theme is not None:
        lines.append(f"config.theme={theme} -> colour palette; not applied")
    return lines
