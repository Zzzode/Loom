fn xychart_visible_high_precision_range_uses_exact_domain_disclosure() {
    let build = |max_value: f64| {
        let mut model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            0.0,
            max_value,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![max_value],
                data: Vec::new(),
                point_labels: Vec::new(),
            }],
        );
        model.display.y_axis = XyChartAxisDisplayPolicy::default();
        model
    };

    let rounded = render_typed_xychart(&build(10.0), &AsciiRenderOptions::ascii())
        .expect("the integer range should render");
    let precise = render_typed_xychart(&build(10.0000001), &AsciiRenderOptions::ascii())
        .expect("the high-precision range should render");

    assert_ne!(rounded, precise);
    assert!(
        precise.contains("yDomain: linear authored=[0,10.0000001] resolved=[0,10.0000001]"),
        "{precise}"
    );
}