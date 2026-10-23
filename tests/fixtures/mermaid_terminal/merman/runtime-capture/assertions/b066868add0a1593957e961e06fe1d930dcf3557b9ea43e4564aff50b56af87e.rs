fn state_simple_transition_renders_through_render_model() {
    let rendered = render_state("stateDiagram-v2\nA --> B: go", &AsciiRenderOptions::ascii())
        .expect("simple state transition should render");

    assert!(
        rendered.lines().any(|line| line.contains("| A")),
        "source state should render as a graph node:\n{rendered}"
    );
    assert!(
        rendered.contains("go"),
        "transition label should render on the graph edge:\n{rendered}"
    );
    assert!(
        rendered.lines().any(|line| line.contains("| B")),
        "target state should render as a graph node:\n{rendered}"
    );
}