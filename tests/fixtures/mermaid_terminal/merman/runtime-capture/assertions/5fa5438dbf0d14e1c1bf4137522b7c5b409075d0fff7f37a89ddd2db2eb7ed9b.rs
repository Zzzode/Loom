fn sequence_actor_keyword_renders_as_participant_box() {
    let rendered = render_sequence(
        "sequenceDiagram\nactor U as User\nparticipant S as System\nU->>S: Click",
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence actor keyword should render in ASCII");

    assert!(
        rendered.contains("| User |") && rendered.contains("| System |"),
        "actor and participant labels should both render as ASCII participant boxes:\n{rendered}"
    );
    assert!(
        rendered.contains("Click"),
        "messages involving actor declarations should keep rendering:\n{rendered}"
    );
}