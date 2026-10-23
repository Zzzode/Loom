fn sequence_wrapped_boxes_render_multiline_labels() {
    let rendered = render_sequence(
        "sequenceDiagram\nbox :wrap: Alpha Beta Gamma Delta\nparticipant A\nend\nA->>A: Ping",
        &AsciiRenderOptions::ascii(),
    )
    .expect("wrapped sequence boxes should render");
    let normalized = normalize_sequence_output(&rendered);
    let lines = normalized.lines().collect::<Vec<_>>();
    let alpha = lines
        .iter()
        .position(|line| line.contains("Alpha"))
        .expect("first wrapped box label line should render");
    let gamma = lines
        .iter()
        .position(|line| line.contains("Gamma"))
        .expect("second wrapped box label line should render");

    assert_eq!(
        gamma,
        alpha + 1,
        "wrapped box label lines should stay adjacent above participant content:\n{rendered}"
    );
    assert!(
        rendered.contains("Beta") && rendered.contains("Delta"),
        "wrapped box labels should keep all words:\n{rendered}"
    );
    assert!(
        !rendered.contains("Alpha Beta Gamma Delta") && !rendered.contains(":wrap:"),
        "wrapped box label should not render as one long line or leak wrap syntax:\n{rendered}"
    );
}