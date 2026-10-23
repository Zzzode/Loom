fn er_parser_presentation_metadata_is_intentionally_omitted() {
    let baseline = render_er(
        "erDiagram\ndirection LR\nCUSTOMER ||--o{ ORDER : places",
        &AsciiRenderOptions::ascii(),
    )
    .expect("baseline ER relationship should render");
    let decorated = render_er(
        r#"erDiagram
accTitle: My access title
accDescr {
  A long multi-line description block
}
direction LR
%% this comment is intentionally omitted
classDef important fill:#f9f
CUSTOMER:::important ||--o{ ORDER : places
style CUSTOMER color:red"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER presentation metadata should not block terminal rendering");

    assert_eq!(
        decorated, baseline,
        "presentation-only ER metadata should not change the terminal semantic projection"
    );
    for omitted in [
        "My access title",
        "description block",
        "intentionally omitted",
        "important",
        "#f9f",
        "red",
    ] {
        assert!(
            !decorated.contains(omitted),
            "presentation metadata {omitted:?} leaked into ER output:\n{decorated}"
        );
    }
}