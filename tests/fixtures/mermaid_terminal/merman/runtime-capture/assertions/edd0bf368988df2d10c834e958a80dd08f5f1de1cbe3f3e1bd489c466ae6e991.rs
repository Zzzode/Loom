fn flowchart_parser_rejects_internal_icon_layout_shape_names() {
    for shape in ["iconSquare", "iconCircle", "iconRounded", "imageSquare"] {
        let input = format!("flowchart LR\nA@{{ shape: {shape}, label: \"X\" }}");
        let err = parse_flowchart_error(&input);
        assert!(
            err.contains(&format!(
                "No such shape: {shape}. Shape names should be lowercase."
            )),
            "internal layout shape should be rejected before ASCII rendering:\n{err}"
        );
    }
}