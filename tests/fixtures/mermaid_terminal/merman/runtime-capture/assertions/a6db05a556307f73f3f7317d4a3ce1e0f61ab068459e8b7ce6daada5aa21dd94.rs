fn er_attribute_keys_and_comments_have_distinct_terminal_roles() {
    let key = render_er(
        "erDiagram\nA {\n  string email UK\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER key attribute should render");
    let comment = render_er(
        "erDiagram\nA {\n  string email \"UK\"\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER comment attribute should render");

    assert!(
        key.contains(&framed_er_attribute("string", "email", &["UK"], "")),
        "{key}"
    );
    assert!(
        comment.contains(&framed_er_attribute("string", "email", &[], "UK")),
        "{comment}"
    );
    assert_ne!(key, comment, "key and comment semantics must not collide");
}