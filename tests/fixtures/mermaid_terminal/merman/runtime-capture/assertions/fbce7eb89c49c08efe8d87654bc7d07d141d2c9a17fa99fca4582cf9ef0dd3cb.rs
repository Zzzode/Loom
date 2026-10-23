fn class_local_semantic_fixture_covers_dense_relationships() {
    let input = read_local_semantic_fixture("class/dense_relations.mmd");

    let rendered = render_class(&input, &AsciiRenderOptions::ascii())
        .expect("dense local semantic class fixture should render");

    for expected in [
        "Service", "Repo", "Cache", "Logger", "fetch", "read", "trace",
    ] {
        assert!(
            rendered.contains(expected),
            "dense semantic class fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.lines().count() >= 6,
        "dense semantic class fixture should produce a non-trivial multi-line layout:\n{rendered}"
    );
}