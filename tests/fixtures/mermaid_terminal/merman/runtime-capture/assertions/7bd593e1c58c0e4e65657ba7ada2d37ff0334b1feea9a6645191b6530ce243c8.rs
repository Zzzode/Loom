fn class_parser_even_width_layered_label_uses_exact_terminal_extent() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA --> B : evenly\nB --> C : next",
        &AsciiRenderOptions::ascii(),
    )
    .expect("an even-width layered label must not be rejected as a grid overflow");

    for expected in ["A", "B", "C", "evenly", "next"] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?} from exact-width layered output:\n{rendered}"
        );
    }
    assert!(!rendered.contains("relations:"), "{rendered}");
}