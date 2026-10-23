fn class_relationship_labels_preserve_complex_graphemes() {
    let mut model = parse_class_model("classDiagram\nclass A\nclass B\nA --> B : owns");
    model
        .classes
        .get_mut("A")
        .expect("class A should exist")
        .text = "Client 👩‍💻".to_string();
    model.relations[0].title = "owns 👩‍💻".to_string();
    let rendered = render_class_model(&model, &AsciiRenderOptions::ascii())
        .expect("class relationship should render");

    assert!(rendered.contains("Client 👩‍💻"), "{rendered}");
    assert!(rendered.contains("owns 👩‍💻"), "{rendered}");
    assert!(!rendered.contains("relations:"), "{rendered}");
}