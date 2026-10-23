fn xychart_single_quantized_y_value_remains_exact_across_models() {
    let build = |value: f64| {
        typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            0.0,
            100.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![value],
                data: vec![("A".to_string(), Some(value))],
                point_labels: Vec::new(),
            }],
        )
    };

    let y10 = render_typed_xychart(&build(10.0), &AsciiRenderOptions::ascii())
        .expect("the first quantized y model should render");
    let y11 = render_typed_xychart(&build(11.0), &AsciiRenderOptions::ascii())
        .expect("the second quantized y model should render");

    assert_ne!(y10, y11, "quantized y values must remain injective");
    assert!(y10.contains("x(bytes=1)=\"A\" value=10"), "{y10}");
    assert!(y11.contains("x(bytes=1)=\"A\" value=11"), "{y11}");
}