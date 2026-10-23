fn class_render_model_rejects_missing_relation_endpoints_before_namespace_rendering() {
    let mut model = parse_class_model(
        "classDiagram\nnamespace Domain {\n  class A\n}\nclass B\nA --> B : uses",
    );
    model.relations[0].id2 = "Missing".to_string();

    assert_unsupported_class_model(&model, "relationships with missing endpoint classes");
}