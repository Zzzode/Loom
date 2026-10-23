fn class_typed_endpoint_label_cannot_render_as_endpoint_ref_provenance() {
    const SPOOF: &str = r#"endpointRef(bytes=8)="Domain.T""#;

    let mut model = parse_class_model("classDiagram\nclass A\nclass B\nA --> B");
    model.relations[0].relation_title_1 = Some(SPOOF.to_string());
    let rendered = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("typed authored endpoint label should render in its dedicated slot");

    assert!(
        rendered.contains(r#"endpointLabel=[bytes=31 "endpointRef"#),
        "{rendered}"
    );
}