fn flowchart_parser_public_state_pseudo_and_fork_join_shapes_render() {
    let cases = [
        ("fork", false),
        ("join", false),
        ("start", false),
        ("stop", false),
    ];

    for (shape, should_show_label) in cases {
        let label = if shape == "start" { "S" } else { "F" };
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"{label}\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        if should_show_label {
            assert!(
                rendered.contains(label),
                "{shape} shape should keep its label:\n{rendered}"
            );
        } else {
            assert!(
                !rendered.contains(label),
                "{shape} shape should not keep its label:\n{rendered}"
            );
        }
    }
}