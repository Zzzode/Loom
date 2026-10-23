fn xychart_single_bar_quantized_x_remains_exact_across_models() {
    let build = |x: &str| {
        typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Linear {
                title: String::new(),
                min: Some(0.0),
                max: Some(100.0),
            },
            0.0,
            100.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![40.0],
                data: vec![(x.to_string(), Some(40.0))],
                point_labels: Vec::new(),
            }],
        )
    };

    let x10 = render_typed_xychart(&build("10"), &AsciiRenderOptions::ascii())
        .expect("the first single-bar model should render");
    let x20 = render_typed_xychart(&build("20"), &AsciiRenderOptions::ascii())
        .expect("the second single-bar model should render");

    assert_ne!(x10, x20, "quantized bar x values must remain injective");
    assert!(x10.contains("x(bytes=2)=\"10\" value=40"), "{x10}");
    assert!(x20.contains("x(bytes=2)=\"20\" value=40"), "{x20}");
}