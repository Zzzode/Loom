fn class_render_model_rejects_relations_without_endpoint_classes() {
    let mut model = parse_class_model("classDiagram\nclass A\nclass B\nA --> B : uses");
    model.classes.clear();

    assert_unsupported_class_model(&model, "relationships with missing endpoint classes");
}