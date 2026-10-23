fn class_render_model_rejects_missing_note_targets_before_namespace_rendering() {
    let mut model = parse_class_model(
        "classDiagram\nnamespace Domain {\n  class A\n  note for A \"linked\"\n}",
    );
    model.notes[0].class_id = Some("Missing".to_string());

    assert_unsupported_class_model(&model, "notes with missing target classes");
}