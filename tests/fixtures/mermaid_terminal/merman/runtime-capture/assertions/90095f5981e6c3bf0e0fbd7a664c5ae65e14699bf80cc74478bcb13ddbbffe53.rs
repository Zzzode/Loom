fn er_render_model_rejects_relationships_without_endpoint_entities() {
    let mut model = parse_er_model("erDiagram\nA ||--|| B : owns");
    model.entities.clear();

    assert_unsupported_er_model(&model, "relationships with missing endpoint entities");
}