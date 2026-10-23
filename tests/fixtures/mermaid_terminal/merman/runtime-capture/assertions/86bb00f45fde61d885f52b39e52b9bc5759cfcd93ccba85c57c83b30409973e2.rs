fn class_render_model_rejects_inconsistent_namespace_class_ownership() {
    let mut model = parse_class_model("classDiagram\nnamespace Domain {\n  class A\n}");
    model
        .namespaces
        .get_mut("Domain")
        .expect("Domain namespace should exist")
        .class_ids
        .clear();

    assert_unsupported_class_model(&model, "inconsistent class namespace ownership");
}