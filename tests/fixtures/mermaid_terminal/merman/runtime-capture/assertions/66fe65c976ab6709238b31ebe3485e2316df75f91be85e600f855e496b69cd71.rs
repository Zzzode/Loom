fn class_local_semantic_fixture_covers_routed_relationship_variants() {
    let input = read_local_semantic_fixture("class/routed_relationship_variants.mmd");

    let rendered = render_class(&input, &AsciiRenderOptions::ascii())
        .expect("routed relationship variant class fixture should render");

    for expected in [
        "Shape",
        "<<interface>>",
        "Circle",
        "radius",
        "draw",
        "implements",
        "paints",
        "loads",
        "keeps",
        "contains",
    ] {
        assert!(
            rendered.contains(expected),
            "routed relationship variant fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "routed relationship variant fixture should remain a routed grid, not a summary:\n{rendered}"
    );
}