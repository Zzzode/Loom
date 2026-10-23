fn er_parser_complex_styled_example_limits_summary_to_unroutable_component() {
    let rendered = render_er(
        r#"erDiagram
    CAR ||--o{ DRIVER : "insured for"
    CAR }o--|| PERSON : "owned by"
    NODE ||--o{ NODE : "leads to"
    BOOK["Book"]:::core {
      string *title PK "Title"
      string[] author-ref[name](1) FK "Author ref"
    }
    BOOK ||--o{ PAGE : has
    PAGE {
      int number PK
    }
    classDef core fill:#f96,stroke:#333,stroke-width:2px,color:#fff
    class BOOK core"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("complex styled ER example should render");

    for expected in [
        "Book".to_string(),
        framed_er_attribute("string", "*title", &["PK"], "Title"),
        framed_er_attribute("string[]", "author-ref[name](1)", &["FK"], "Author ref"),
        "PAGE".to_string(),
        framed_er_attribute("int", "number", &["PK"], ""),
        "CAR".to_string(),
        "DRIVER".to_string(),
        "PERSON".to_string(),
        "NODE".to_string(),
        "relations:".to_string(),
        format!(
            "{} ||--o{{ {}",
            framed_er_summary_endpoint("CAR"),
            framed_er_summary_endpoint("DRIVER")
        ),
        format!(
            "{} }}o--|| {}",
            framed_er_summary_endpoint("CAR"),
            framed_er_summary_endpoint("PERSON")
        ),
        "insured for".to_string(),
        "owned by".to_string(),
        "leads to".to_string(),
        "has".to_string(),
    ] {
        assert!(
            rendered.contains(&expected),
            "complex styled ER example should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert_eq!(
        rendered.matches("| NODE |").count(),
        1,
        "the routed self relation should keep one entity box:\n{rendered}"
    );
}