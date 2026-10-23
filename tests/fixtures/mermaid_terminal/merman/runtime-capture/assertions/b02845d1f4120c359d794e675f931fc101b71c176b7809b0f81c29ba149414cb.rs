fn sequence_local_semantic_fixture_covers_dense_control_rows() {
    let input = read_local_semantic_fixture("sequence/dense_control_rows.mmd");

    let rendered = render_sequence(&input, &AsciiRenderOptions::unicode())
        .expect("dense local semantic sequence fixture should render");

    for expected in [
        "Outer Work",
        "Coordinate",
        "Parallel Branches",
        "Fallback",
        "Retry",
        "Stop",
    ] {
        assert!(
            rendered.contains(expected),
            "dense semantic sequence fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.contains('┃'),
        "dense semantic sequence fixture should keep active lifelines visible:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 10,
        "dense semantic sequence fixture should produce a multi-line layout:\n{rendered}"
    );
}