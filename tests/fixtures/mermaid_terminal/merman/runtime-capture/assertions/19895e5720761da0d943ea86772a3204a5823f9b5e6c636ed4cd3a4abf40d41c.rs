fn class_render_model_rejects_inconsistent_namespace_note_ownership() {
    let mut model = parse_class_model(
        "classDiagram\nnamespace Domain {\n  class A\n  note for A \"linked\"\n}",
    );
    model
        .namespaces
        .get_mut("Domain")
        .expect("Domain namespace should exist")
        .note_ids
        .clear();

    assert_unsupported_class_model(&model, "inconsistent class namespace ownership");
}