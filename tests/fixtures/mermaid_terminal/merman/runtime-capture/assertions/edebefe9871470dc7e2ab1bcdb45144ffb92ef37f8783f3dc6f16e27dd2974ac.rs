fn er_render_model_rejects_duplicate_rendered_entity_ids() {
    let mut model = parse_er_model("erDiagram\nA ||--|| B : owns");
    model.relationships.clear();
    let duplicate_id = model
        .entities
        .get("A")
        .expect("entity A should exist")
        .id
        .clone();
    model
        .entities
        .get_mut("B")
        .expect("entity B should exist")
        .id = duplicate_id.clone();
    assert_eq!(
        model
            .entities
            .values()
            .filter(|entity| entity.id == duplicate_id)
            .count(),
        2,
        "test model should contain two map entries with the same rendered id",
    );

    assert_unsupported_er_model(&model, "duplicate rendered ER entity ids");
}