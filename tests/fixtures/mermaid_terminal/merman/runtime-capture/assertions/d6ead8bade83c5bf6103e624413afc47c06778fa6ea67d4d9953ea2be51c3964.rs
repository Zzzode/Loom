fn flowchart_parser_datastore_shape_renders() {
    for shape in ["datastore", "data-store"] {
        let rendered = render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Store\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap();

        assert!(
            rendered.contains("Store"),
            "datastore shape should keep its label"
        );
        assert!(
            rendered.lines().any(|line| line.contains("Store")),
            "datastore shape should render as a box:\n{rendered}"
        );
    }
}