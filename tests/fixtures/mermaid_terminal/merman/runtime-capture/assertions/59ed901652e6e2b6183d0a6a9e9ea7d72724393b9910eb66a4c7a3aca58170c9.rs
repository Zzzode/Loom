fn er_relationship_labels_preserve_complex_graphemes() {
    let mut model = parse_er_model("erDiagram\nA ||--|| B : owns");
    model
        .entities
        .get_mut("A")
        .expect("entity A should exist")
        .alias = "Client 👩‍💻".to_string();
    model.relationships[0].role_a = "owns 👩‍💻".to_string();
    let rendered = render_er_model(&model, &AsciiRenderOptions::ascii())
        .expect("ER relationship should render");

    assert!(rendered.contains("Client 👩‍💻"), "{rendered}");
    assert!(rendered.contains("owns 👩‍💻"), "{rendered}");
    assert!(!rendered.contains("relations:"), "{rendered}");
}