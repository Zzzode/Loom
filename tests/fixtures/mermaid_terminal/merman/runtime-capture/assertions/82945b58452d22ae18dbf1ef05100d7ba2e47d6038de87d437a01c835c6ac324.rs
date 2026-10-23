fn er_local_semantic_fixture_covers_dense_relationships() {
    let input = read_local_semantic_fixture("er/dense_relations.mmd");

    let rendered = render_er(&input, &AsciiRenderOptions::ascii())
        .expect("dense local semantic ER fixture should render");

    for expected in [
        "CUSTOMER", "ORDER", "INVOICE", "places", "billed", "invoices",
    ] {
        assert!(
            rendered.contains(expected),
            "dense semantic ER fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.lines().count() >= 6,
        "dense semantic ER fixture should produce a multi-line layout:\n{rendered}"
    );
}