fn xychart_inferred_visible_range_discloses_authored_and_resolved_identity() {
    let build = |max_value: f64| {
        let mut model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string(), "B".to_string()],
            },
            0.0,
            max_value,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![0.0, max_value],
                data: Vec::new(),
                point_labels: Vec::new(),
            }],
        );
        model.y_axis = XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: None,
            max: None,
        };
        model.display.y_axis = XyChartAxisDisplayPolicy::default();
        model
    };

    let ten = render_typed_xychart(&build(10.0), &AsciiRenderOptions::ascii())
        .expect("the first inferred range should render");
    let twenty = render_typed_xychart(&build(20.0), &AsciiRenderOptions::ascii())
        .expect("the second inferred range should render");

    assert_ne!(ten, twenty);
    assert!(
        ten.contains("yDomain: linear authored=[none,none] resolved=[0,10]"),
        "{ten}"
    );
    assert!(
        twenty.contains("yDomain: linear authored=[none,none] resolved=[0,20]"),
        "{twenty}"
    );
}