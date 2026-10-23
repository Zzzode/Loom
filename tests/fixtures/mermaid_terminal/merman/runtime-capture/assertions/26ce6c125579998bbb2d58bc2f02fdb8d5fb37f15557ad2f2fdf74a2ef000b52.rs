fn er_parser_attribute_keys_and_comments_render_in_entity_section() {
    let rendered = render_er(
        "erDiagram\nORDER {\n  int id PK\n  int customer_id FK \"owner id\"\n  string email UK\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER should render");

    for expected in [
        framed_er_attribute("int", "id", &["PK"], ""),
        framed_er_attribute("int", "customer_id", &["FK"], "owner id"),
        framed_er_attribute("string", "email", &["UK"], ""),
    ] {
        assert!(
            rendered.contains(&expected),
            "ER attribute details should keep {expected:?} visible:\n{rendered}"
        );
    }
}