fn er_local_semantic_fixture_covers_routed_schema_with_attributes() {
    let input = read_local_semantic_fixture("er/routed_schema_with_attributes.mmd");

    let rendered = render_er(&input, &AsciiRenderOptions::ascii())
        .expect("routed schema ER fixture should render");

    for expected in [
        "CUSTOMER".to_string(),
        "ORDER".to_string(),
        "LINE_ITEM".to_string(),
        "PRODUCT".to_string(),
        framed_er_attribute("string", "id", &["PK"], ""),
        framed_er_attribute("string", "email", &["UK"], ""),
        framed_er_attribute("int", "quantity", &[], ""),
        "places".to_string(),
        "contains".to_string(),
        "supplies".to_string(),
    ] {
        assert!(
            rendered.contains(&expected),
            "routed schema fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.contains("relations:"),
        "conflicting LINE_ITEM cardinalities should use the lossless summary:\n{rendered}"
    );
}