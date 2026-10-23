fn flowchart_parser_shape_data_pipeline_example_renders_readable_ascii_shapes() {
    let rendered = render_flowchart(
        r#"flowchart LR
    Source@{ shape: lean-r, label: "Event stream" } --> Load[Normalize]
    Load --> Store@{ shape: datastore, label: "Warehouse" }
    Store --> Report@{ shape: doc, label: "Daily report" }"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("shape-data pipeline example should render");

    for expected in ["Event stream", "Normalize", "Warehouse", "Daily report"] {
        assert!(
            rendered.contains(expected),
            "shape-data pipeline should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered.lines().any(|line| line.starts_with(" /")),
        "lean-r shape should keep its left slant continuous below the top edge:\n{rendered}"
    );
    assert!(
        rendered.contains("~~~~~~~~~~~~~"),
        "document shape should keep the folded bottom edge:\n{rendered}"
    );
}