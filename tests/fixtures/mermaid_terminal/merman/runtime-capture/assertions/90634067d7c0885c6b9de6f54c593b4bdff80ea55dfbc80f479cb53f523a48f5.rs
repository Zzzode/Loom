fn state_local_semantic_fixture_covers_composite_boundaries() {
    let input = read_local_semantic_fixture("state/composite_boundary.mmd");

    let rendered = render_state(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic state fixture should render");

    for expected in ["Outer", "Ready", "Busy", "Idle"] {
        assert!(
            rendered.contains(expected),
            "local semantic state fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.lines().count() >= 5,
        "local semantic state fixture should produce a multi-line layout:\n{rendered}"
    );
}