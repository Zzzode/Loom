fn xychart_local_semantic_fixture_covers_small_mixed_plot() {
    let input = read_local_semantic_fixture("xychart/mixed_small.mmd");

    let rendered = render_xychart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic xychart fixture should render");

    for expected in ["Ops", "A", "B", "C"] {
        assert!(
            rendered.contains(expected),
            "local semantic xychart fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.lines().count() >= 5,
        "local semantic xychart fixture should produce a multi-line layout:\n{rendered}"
    );
}