fn sequence_box_hex_color_is_not_treated_as_drawable_background() {
    let model = parse_sequence_render_model(
        "sequenceDiagram\nbox #112233 Group\nparticipant A\nend\nA->>A: Self",
    );

    assert_eq!(model.boxes.len(), 1);
    assert_eq!(model.boxes[0].fill, "transparent");
    assert!(model.boxes[0].name.is_none());
}