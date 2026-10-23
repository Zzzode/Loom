fn class_parser_independent_relation_pairs_render_without_shared_summary_state() {
    let options = AsciiRenderOptions::ascii();

    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nclass D\nA --> B : ab\nC --> D : cd",
        &options,
    )
    .expect("independent relation pairs should render separately");

    for expected in ["A", "B", "C", "D", "ab", "cd"] {
        assert!(
            rendered.contains(expected),
            "independent class relation pairs should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "independent class relation pairs should remain routed without shared summary state:\n{rendered}"
    );
}