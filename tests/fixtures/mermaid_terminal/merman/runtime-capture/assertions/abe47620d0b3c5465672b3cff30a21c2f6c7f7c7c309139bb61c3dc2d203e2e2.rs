fn er_render_model_rejects_unknown_cardinality_markers() {
    let mut model = parse_er_model("erDiagram\nA ||--|| B : relates");
    model
        .relationships
        .first_mut()
        .expect("fixture should contain one relationship")
        .rel_spec
        .card_a = "MANY".to_string();

    assert_unsupported_er_model(&model, "unknown ER cardinality markers");
}