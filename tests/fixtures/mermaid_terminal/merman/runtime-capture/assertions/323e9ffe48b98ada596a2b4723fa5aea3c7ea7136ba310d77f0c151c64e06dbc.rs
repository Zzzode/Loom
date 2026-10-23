fn flowchart_parser_filled_circle_and_text_shapes_preserve_upstream_label_policy() {
    for shape in ["f-circ", "junction", "filled-circle"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Hidden\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();
        assert!(!rendered.contains("Hidden"), "shape: {shape}\n{rendered}");
    }

    let label_rect = render_flowchart(
        "flowchart LR\nA@{ shape: text, label: \"Label only\" }",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();
    assert_eq!(label_rect.trim(), "Label only");
}