fn class_render_model_rejects_missing_interface_targets() {
    let mut model = parse_class_model("classDiagram\nIService ()-- Service");
    model.interfaces[0].class_id = "Missing".to_string();

    assert_unsupported_class_model(&model, "interfaces with missing target classes");
}