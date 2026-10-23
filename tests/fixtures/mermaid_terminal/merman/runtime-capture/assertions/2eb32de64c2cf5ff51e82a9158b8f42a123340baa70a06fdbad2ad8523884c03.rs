fn class_render_model_rejects_duplicate_rendered_ids() {
    let mut model = parse_class_model("classDiagram\nclass A\nclass B");
    model.classes.get_mut("B").expect("class B should exist").id = "A".to_string();

    assert_unsupported_class_model(&model, "duplicate rendered class ids");
}