fn state_labels_do_not_use_flowchart_node_label_wrap_width() {
    let input = "stateDiagram-v2\nstate \"Alpha Beta Gamma Delta\" as A";
    let rendered = render_state(
        input,
        &AsciiRenderOptions::ascii().with_flowchart_node_label_wrap_width(8),
    )
    .expect("state label should render");

    assert!(
        rendered.contains("Alpha Beta Gamma Delta"),
        "flowchart label wrapping must not affect state nodes:\n{rendered}"
    );
}