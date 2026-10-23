fn er_local_semantic_fixture_covers_attributes_with_relationship() {
    let input = read_local_semantic_fixture("er/attributes_with_relationship.mmd");
    let rendered = render_er(&input, &AsciiRenderOptions::ascii())
        .expect("ER attribute and relationship fixture should render");

    for expected in [
        "CUSTOMER".to_string(),
        "ORDER".to_string(),
        framed_er_attribute("string", "name", &["PK"], ""),
        framed_er_attribute("string", "email", &["UK"], ""),
        framed_er_attribute("int", "age", &[], ""),
        framed_er_attribute("int", "id", &["PK"], ""),
        framed_er_attribute("string", "status", &[], ""),
        "places".to_string(),
        "||".to_string(),
        "o{".to_string(),
    ] {
        assert!(
            rendered.contains(&expected),
            "ER attribute fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    assert!(
        first_line_index_containing(&rendered, "CUSTOMER")
            < first_line_index_containing(&rendered, "ORDER"),
        "identifying relationship should keep CUSTOMER before ORDER in the routed terminal layout:\n{rendered}"
    );
}