fn xychart_hidden_axis_ranges_are_disclosed_exactly() {
    let build = |y_min: f64, y_max: f64| {
        typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            y_min,
            y_max,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![4.0],
                data: vec![("A".to_string(), Some(4.0))],
                point_labels: Vec::new(),
            }],
        )
    };

    let first = render_typed_xychart(&build(0.0, 10.0), &AsciiRenderOptions::ascii())
        .expect("the first hidden range should render");
    let second = render_typed_xychart(&build(-4.0, 16.0), &AsciiRenderOptions::ascii())
        .expect("the second hidden range should render");

    assert_ne!(first, second);
    assert!(
        first.contains("yDomain: linear authored=[0,10] resolved=[0,10]"),
        "{first}"
    );
    assert!(
        second.contains("yDomain: linear authored=[-4,16] resolved=[-4,16]"),
        "{second}"
    );
}