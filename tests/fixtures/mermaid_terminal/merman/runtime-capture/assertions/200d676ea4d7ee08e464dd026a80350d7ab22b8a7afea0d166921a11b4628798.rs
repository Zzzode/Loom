fn er_direct_attribute_fields_cannot_forge_renderer_owned_delimiters() {
    let render = |attribute: ErAttributeRenderModel| {
        let mut model = parse_er_model("erDiagram\nA");
        model
            .entities
            .get_mut("A")
            .expect("entity A should exist")
            .attributes = vec![attribute];
        render_er_model(&model, &AsciiRenderOptions::ascii())
            .expect("direct ER attribute should render")
    };

    let authored_key_syntax = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner [keys: PK]".to_string(),
        keys: Vec::new(),
        comment: String::new(),
    });
    let typed_key = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner".to_string(),
        keys: vec!["PK".to_string()],
        comment: String::new(),
    });
    assert_ne!(authored_key_syntax, typed_key);
    assert!(
        authored_key_syntax.contains(&framed_er_attribute("string", "owner [keys: PK]", &[], "")),
        "{authored_key_syntax}"
    );
    assert!(
        typed_key.contains(&framed_er_attribute("string", "owner", &["PK"], "")),
        "{typed_key}"
    );

    let authored_type_separator = render(ErAttributeRenderModel {
        ty: "string owner".to_string(),
        name: "id".to_string(),
        keys: Vec::new(),
        comment: String::new(),
    });
    let typed_name_separator = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner id".to_string(),
        keys: Vec::new(),
        comment: String::new(),
    });
    assert_ne!(authored_type_separator, typed_name_separator);
    assert!(
        authored_type_separator.contains(&framed_er_attribute("string owner", "id", &[], "")),
        "{authored_type_separator}"
    );
    assert!(
        typed_name_separator.contains(&framed_er_attribute("string", "owner id", &[], "")),
        "{typed_name_separator}"
    );

    let authored_comment_syntax = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner [comment: note]".to_string(),
        keys: Vec::new(),
        comment: String::new(),
    });
    let typed_comment = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner".to_string(),
        keys: Vec::new(),
        comment: "note".to_string(),
    });
    assert_ne!(authored_comment_syntax, typed_comment);
    assert!(
        authored_comment_syntax.contains(&framed_er_attribute(
            "string",
            "owner [comment: note]",
            &[],
            ""
        )),
        "{authored_comment_syntax}"
    );
    assert!(
        typed_comment.contains(&framed_er_attribute("string", "owner", &[], "note")),
        "{typed_comment}"
    );

    let authored_key_separator = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner".to_string(),
        keys: vec!["PK,FK".to_string()],
        comment: String::new(),
    });
    let two_typed_keys = render(ErAttributeRenderModel {
        ty: "string".to_string(),
        name: "owner".to_string(),
        keys: vec!["PK".to_string(), "FK".to_string()],
        comment: String::new(),
    });
    assert_ne!(authored_key_separator, two_typed_keys);
    assert!(
        authored_key_separator.contains(r#"keys=[bytes=5 "PK,FK"]"#),
        "{authored_key_separator}"
    );
    assert!(
        two_typed_keys.contains(r#"keys=[bytes=2 "PK", bytes=2 "FK"]"#),
        "{two_typed_keys}"
    );
}