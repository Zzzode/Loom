fn flowchart_issue_53_long_node_labels_wrap_before_layout() {
    let input = local_semantic_input("flowchart/issue_53_long_node_labels.mmd");

    for options in [AsciiRenderOptions::ascii(), AsciiRenderOptions::unicode()] {
        let rendered = render_flowchart(&input, &options).expect("Issue #53 should render");

        assert_issue_53_semantics(&rendered);
        assert!(
            rendered_terminal_width(&rendered) <= 100,
            "default terminal wrapping should keep Issue #53 readable:\n{rendered}"
        );
        assert_rectangular_terminal_grid(&rendered);
    }
}