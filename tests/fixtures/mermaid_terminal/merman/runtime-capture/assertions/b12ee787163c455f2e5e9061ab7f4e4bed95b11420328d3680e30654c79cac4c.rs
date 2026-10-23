fn sequence_empty_boxes_render_as_diagram_wide_regions() {
    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    model.boxes.push(SequenceBox {
        actor_keys: Vec::new(),
        fill: "green".to_string(),
        name: Some("System boundary".to_string()),
        wrap: false,
    });
    model.messages.push(message(Some("A"), Some("B"), 0));

    let rendered = render_sequence_model(&model, &AsciiRenderOptions::ascii())
        .expect("empty sequence boxes should render as diagram-wide regions");

    assert!(
        rendered.contains("System boundary"),
        "empty box title should remain visible:\n{rendered}"
    );
    assert!(
        rendered.contains("Hi"),
        "empty box should preserve sequence contents:\n{rendered}"
    );
    assert!(
        rendered.contains("+"),
        "empty box should render a terminal region border:\n{rendered}"
    );
}