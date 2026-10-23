fn xychart_linear_and_band_axes_have_distinct_owner_disclosure() {
    let build = |x_axis| {
        let mut model = typed_xychart_model(
            "horizontal",
            x_axis,
            0.0,
            1.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![0.0, 1.0],
                data: Vec::new(),
                point_labels: Vec::new(),
            }],
        );
        model.display.x_axis = XyChartAxisDisplayPolicy::default();
        model.display.y_axis = XyChartAxisDisplayPolicy::default();
        model
    };

    let band = render_typed_xychart(
        &build(XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["0".to_string(), "1".to_string()],
        }),
        &AsciiRenderOptions::ascii(),
    )
    .expect("the Band-axis model should render");
    let linear = render_typed_xychart(
        &build(XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: Some(0.0),
            max: Some(1.0),
        }),
        &AsciiRenderOptions::ascii(),
    )
    .expect("the Linear-axis model should render");

    assert_ne!(band, linear, "axis kinds must remain injective");
    assert!(
        linear.contains("xDomain: linear authored=[0,1] resolved=[0,1]"),
        "{linear}"
    );
}