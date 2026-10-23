fn state_lr_direction_renders_states_on_one_row() {
    let rendered = render_state(
        "stateDiagram-v2\ndirection LR\nA --> B: go",
        &AsciiRenderOptions::ascii(),
    )
    .expect("LR state direction should render");

    assert!(
        rendered
            .lines()
            .any(|line| line.contains("| A |") && line.contains("| B |")),
        "LR state output should place source and target on the same row:\n{rendered}"
    );
}