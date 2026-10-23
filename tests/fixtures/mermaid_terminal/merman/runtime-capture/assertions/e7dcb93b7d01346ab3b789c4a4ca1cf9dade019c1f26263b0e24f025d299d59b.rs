fn xychart_single_series_title_remains_visible() {
    let render = |title: Option<&str>| {
        let model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: title.map(str::to_string),
                values: vec![5.0],
                data: vec![("A".to_string(), Some(5.0))],
                point_labels: Vec::new(),
            }],
        );
        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("single-series XYChart should render")
    };

    let titled = render(Some("Revenue"));
    let untitled = render(None);

    assert!(
        titled.contains("Revenue"),
        "missing series title:\n{titled}"
    );
    assert_ne!(
        titled, untitled,
        "an authored series title must not collapse into an untitled series"
    );
}