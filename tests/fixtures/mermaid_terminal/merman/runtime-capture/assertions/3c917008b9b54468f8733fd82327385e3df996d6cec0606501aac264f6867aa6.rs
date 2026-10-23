fn sequence_box_with_lifecycle_and_mirror_keeps_boundaries() {
    let mut model = basic_sequence_model();
    add_sequence_participant(&mut model, "B");
    add_sequence_participant(&mut model, "C");
    model.boxes.push(SequenceBox {
        actor_keys: vec!["A".to_string(), "B".to_string(), "C".to_string()],
        fill: "green".to_string(),
        name: Some("Group".to_string()),
        wrap: false,
    });
    model.messages.push(SequenceMessage {
        id: "m0".to_string(),
        from: None,
        to: None,
        message_type: LINETYPE_LOOP_START,
        message: SequenceMessagePayload::Text("Setup".to_string()),
        wrap: false,
        activate: false,
        placement: None,
        central_connection: 0,
    });
    model.messages.push(SequenceMessage {
        id: "m1".to_string(),
        from: Some("B".to_string()),
        to: Some("C".to_string()),
        message_type: 0,
        message: SequenceMessagePayload::Text("Hello C".to_string()),
        wrap: false,
        activate: false,
        placement: None,
        central_connection: 0,
    });
    model.messages.push(SequenceMessage {
        id: "m2".to_string(),
        from: Some("C".to_string()),
        to: Some("B".to_string()),
        message_type: 0,
        message: SequenceMessagePayload::Text("Still here".to_string()),
        wrap: false,
        activate: false,
        placement: None,
        central_connection: 0,
    });
    model.messages.push(SequenceMessage {
        id: "m3".to_string(),
        from: Some("B".to_string()),
        to: Some("C".to_string()),
        message_type: 6,
        message: SequenceMessagePayload::Text("Bye C".to_string()),
        wrap: false,
        activate: false,
        placement: None,
        central_connection: 0,
    });
    model.messages.push(empty_message(LINETYPE_LOOP_END));
    model.created_actors.insert("C".to_string(), 1);
    model.destroyed_actors.insert("C".to_string(), 3);

    let rendered = render_sequence_model(
        &model,
        &AsciiRenderOptions::ascii().with_sequence_mirror_actors(true),
    )
    .expect("boxed lifecycle control block with mirrored actors should render");

    for expected in ["Group", "loop Setup", "Hello C", "Still here", "Bye C"] {
        assert!(
            rendered.contains(expected),
            "boxed lifecycle sequence should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.matches("| C |").count() == 1,
        "created then destroyed actor should render at lifecycle point, not in the final mirror footer:\n{rendered}"
    );
    assert!(
        rendered.contains('x'),
        "destroyed actor should render a termination marker:\n{rendered}"
    );
    let frame_top = rendered
        .lines()
        .find(|line| line.contains("loop Setup"))
        .unwrap_or_else(|| panic!("the lifecycle control frame should render:\n{rendered}"));
    let outer_border = frame_top
        .find('|')
        .unwrap_or_else(|| panic!("the outer sequence box should render:\n{rendered}"));
    let frame_left = frame_top
        .find('+')
        .unwrap_or_else(|| panic!("the inner control frame should render:\n{rendered}"));
    assert!(
        frame_left >= outer_border + 2,
        "control frame should keep padding inside the outer sequence box:\n{rendered}"
    );
    assert!(
        !rendered.lines().any(|line| line.starts_with("|+")),
        "outer sequence box and inner frame borders should not merge:\n{rendered}"
    );
    assert!(
        !rendered.lines().any(|line| line.starts_with("||")),
        "outer sequence box and participant or frame borders should not merge:\n{rendered}"
    );
}