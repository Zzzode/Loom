fn flowchart_parser_rejects_remaining_uncommon_shapes() {
    let shape = "icon";
    let input = format!("flowchart LR\nA@{{ shape: {shape}, label: \"X\" }}");
    let err = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect_err("unsupported shape should be rejected");
    assert!(matches!(
        err,
        merman_ascii::AsciiError::UnsupportedFeature {
            diagram_type: "flowchart",
            feature: "unsupported flowchart node shape projections",
        }
    ));
}