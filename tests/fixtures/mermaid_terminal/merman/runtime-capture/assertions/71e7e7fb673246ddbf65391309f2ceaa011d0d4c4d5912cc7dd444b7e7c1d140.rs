fn xychart_empty_series_preserves_exact_y_domain_identity() {
    let build = |max_value: f64| {
        let mut model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["ABCD".to_string()],
            },
            0.0,
            max_value,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: Vec::new(),
                data: Vec::new(),
                point_labels: Vec::new(),
            }],
        );
        model.display.y_axis = XyChartAxisDisplayPolicy::default();
        model
    };

    let rounded = render_typed_xychart(&build(10.0), &AsciiRenderOptions::ascii())
        .expect("the rounded empty-series range should render");
    let precise = render_typed_xychart(&build(10.0000001), &AsciiRenderOptions::ascii())
        .expect("the precise empty-series range should render");

    assert_ne!(
        rounded, precise,
        "empty series must retain Y-domain identity"
    );
    assert!(
        precise.contains("yDomain: linear authored=[0,10.0000001] resolved=[0,10.0000001]"),
        "{precise}"
    );
}