fn state_choice_pseudo_state_renders_without_internal_id() {
    let rendered = render_state(
        "stateDiagram-v2\nstate choice_state <<choice>>\n[*] --> choice_state\nchoice_state --> A: yes\nchoice_state --> B: no",
        &AsciiRenderOptions::ascii(),
    )
    .expect("choice pseudo state should render");

    assert!(
        rendered.contains("yes") && rendered.contains("no"),
        "choice branch labels should render on outgoing edges:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.contains('<') && line.contains('>')),
        "choice pseudo state should render as a visible diamond-like node:\n{rendered}"
    );
    assert!(
        !rendered.contains("choice_state"),
        "choice implementation id should not leak into ASCII output:\n{rendered}"
    );
}