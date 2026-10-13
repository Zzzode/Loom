#!/usr/bin/env python3
"""Guard UI chrome colors and icons; preserve explicit palettes and color protocols."""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODULES = tuple(sorted(str(p.relative_to(ROOT)) for p in (ROOT / "src/ui").rglob("*")
                       if p.suffix in (".cpp", ".cppm")))
DEFINITION_FILES = {
    "src/ui/foundation/design_tokens.cppm",
    "src/ui/foundation/design_tokens.cpp",  # Out-of-line palette definitions.
    "src/ui/foundation/design_figures.cppm",
}
COLOR_FACTORIES = {
    "src/ui/foundation/theme_provider.cppm": ("resolve_color",),
    "src/ui/chrome/ansi_render.cpp": ("sgr_color_value_to_ftxui",),
    "src/ui/visual/code_highlight.cppm": ("get_theme",),
    "src/ui/visual/structured_diff.cppm": ("get_diff_syntax_theme",),
}
TOKENS = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
COLORS = re.compile(r"\b(?:ftxui\s*::\s*)?Color\s*::\s*\w+")


def decode_string(literal: str) -> str:
    """Decode UTF-8 and C++ hexadecimal/Unicode escapes without executing text."""
    source = literal[1:-1]
    data = bytearray()
    index = 0
    while index < len(source):
        if source[index] == "\\" and index + 1 < len(source):
            escape = source[index + 1]
            if escape == "x":
                match = re.match(r"[0-9a-fA-F]+", source[index + 2:])
                if match:
                    data.append(int(match[0], 16) & 0xff)
                    index += 2 + len(match[0])
                    continue
            if escape in ("u", "U"):
                length = 4 if escape == "u" else 8
                digits = source[index + 2:index + 2 + length]
                if len(digits) == length and re.fullmatch(r"[0-9a-fA-F]+", digits):
                    data.extend(chr(int(digits, 16)).encode("utf-8"))
                    index += length + 2
                    continue
            # Other escapes do not introduce pictographs.
            index += 2
            continue
        data.extend(source[index].encode("utf-8"))
        index += 1
    return data.decode("utf-8", errors="replace")


def violations(source: str, color_factories: tuple[str, ...] = ()) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    # Keep offsets while masking comments/literals, so braces inside strings
    # cannot extend the narrow notification-only exceptions into other functions.
    code = TOKENS.sub(lambda token: re.sub(r"[^\n]", " ", token[0]), source)
    allowed_ranges = []
    for name in color_factories:
        for function in re.finditer(rf"\b{name}\s*\([^{{}};]*\)\s*(?:noexcept\s*)?\{{", code):
            start = function.end() - 1
            depth = 1
            end = start + 1
            while end < len(code) and depth:
                depth += (code[end] == "{") - (code[end] == "}")
                end += 1
            if depth == 0:
                allowed_ranges.append((start, end))
    for token in TOKENS.finditer(source):
        value = token[0]
        line = source.count("\n", 0, token.start()) + 1
        if value.startswith(('"', "'")):
            for glyph in decode_string(value):
                point = ord(glyph)
                if 0x1f000 <= point <= 0x1faff or point == 0xfe0f or point in (0x270f, 0x26a1, 0x2699, 0x23f1, 0x23f8, 0x2b06, 0x2b07, 0x2601):
                    result.append((line, f"decorative emoji U+{point:04X}; use figures or text"))
    for color in COLORS.finditer(code):
        if any(start < color.start() < end for start, end in allowed_ranges):
            continue
        result.append((source.count("\n", 0, color.start()) + 1,
                       f"concrete color {color[0]}; use the current theme palette"))
    return result


def main() -> int:
    cases = (
        ("return Color::Red;", (), True),
        ("fg = ftxui :: Color :: Green;", (), True),
        ("Color{Color::Palette16::Red};", (), True),
        ('color(Color::RGB(1, 2, 3));', (), True),
        ('text("🤖");', (), True),
        (r'text("\xF0\x9F\xA4\x96");', (), True),
        (r'text("\U0001F916");', (), True),
        ('text("✏️");', (), True),
        ('text("⏸⏱⬇⬆☁");', (), True),
        ('// Color::Red 🤖\ncolor(pal.success);', (), False),
        ('text("Color::Red"); Color{};', (), False),
        ('text("🔑🎁");', (), True),
        ('Color get_theme(int n) { return Color::Red; }', ("get_theme",), False),
        ('Color get_theme(int n) { return Color::Red; } color(Color::Red);', ("get_theme",), True),
        ('Color get_theme(int n) { text("}🤖{"); return Color::Red; }', ("get_theme",), True),
        ('Color other() { return Color::Red; }', ("get_theme",), True),
    )
    for source, factories, fails in cases:
        if bool(violations(source, factories)) != fails:
            print(f"lint negative control failed: {source}", file=sys.stderr)
            return 1
    errors = 0
    for module in MODULES:
        if module in DEFINITION_FILES:
            continue
        for line, reason in violations((ROOT / module).read_text(encoding="utf-8"),
                color_factories=COLOR_FACTORIES.get(module, ())):
            print(f"{module}:{line}: {reason}")
            errors += 1
    print(f"UI theme lint: {len(MODULES)} modules, {errors} violations; negative controls passed")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
