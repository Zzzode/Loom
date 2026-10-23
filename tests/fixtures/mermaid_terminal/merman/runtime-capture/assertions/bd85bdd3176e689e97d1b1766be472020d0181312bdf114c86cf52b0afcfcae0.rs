fn xychart_exact_disclosure_is_injective_for_authored_delimiters() {
    let split_fields = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string(), "B".to_string()],
        },
        0.0,
        2.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Bar,
            title: None,
            values: vec![0.0, 2.0],
            data: vec![("A".to_string(), Some(0.0)), ("B".to_string(), Some(2.0))],
            point_labels: vec![String::new(), "p".to_string()],
        }],
    );
    let authored_delimiters = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["unused".to_string(), "A=0, B".to_string()],
        },
        0.0,
        2.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Bar,
            title: None,
            values: vec![2.0],
            data: vec![("A=0, B".to_string(), Some(2.0))],
            point_labels: vec!["p".to_string()],
        }],
    );

    let split_fields = render_typed_xychart(&split_fields, &AsciiRenderOptions::ascii())
        .expect("split XYChart fields should render");
    let authored_delimiters =
        render_typed_xychart(&authored_delimiters, &AsciiRenderOptions::ascii())
            .expect("authored XYChart delimiters should render");

    assert_ne!(
        split_fields, authored_delimiters,
        "authored delimiters must not forge disclosure field ownership"
    );
}