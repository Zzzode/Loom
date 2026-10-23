fn class_routed_aliases_disclose_authored_endpoint_identities() {
    let rendered = render_class(
        "classDiagram\nclass A[\"X\"]\nclass B[\"X\"]\nA --> B : calls",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class aliases with one relation should render diagrammatically");

    assert!(!rendered.contains("relations:"), "{rendered}");
    for identity in [r#"id(bytes=1)="A""#, r#"id(bytes=1)="B""#] {
        assert!(
            rendered.contains(identity),
            "routed class aliases must preserve {identity:?}:\n{rendered}"
        );
    }
}