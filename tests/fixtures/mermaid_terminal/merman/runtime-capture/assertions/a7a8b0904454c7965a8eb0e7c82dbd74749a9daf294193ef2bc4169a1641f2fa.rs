fn er_render_model_rejects_unknown_relationship_identification_types() {
    let mut model = parse_er_model("erDiagram\nA ||--|| B : relates");
    model
        .relationships
        .first_mut()
        .expect("fixture should contain one relationship")
        .rel_spec
        .rel_type = "NEITHER".to_string();

    assert_unsupported_er_model(&model, "unknown ER relationship identification types");
}