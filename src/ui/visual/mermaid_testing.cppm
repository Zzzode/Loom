// Test-only facade (RFC 0031 E6a). This is the single module name Mermaid
// test translation units import; it makes the reviewed
// loom::ui::mermaid::testing seam and the public render API reachable while
// the loom::ui::mermaid::detail partition stays unnameable.
//
// The re-export must be the WIDE `export import` form: clang 23 does not
// transitively expose complete class definitions through narrow exported
// using-declarations, and the tests instantiate Work and read struct fields.
// Production code never imports this module.
export module loom.ui.visual.mermaid.testing;

export import loom.ui.visual.mermaid;
