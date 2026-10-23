fn xychart_single_linear_sample_preserves_authored_x_across_models() {
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
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![50.0],
                data: vec![(x.to_string(), Some(50.0))],
                point_labels: Vec::new(),
            }],
        )
    };

    let x10 = render_typed_xychart(&build("10"), &AsciiRenderOptions::ascii())
        .expect("the first single-sample model should render");
    let x20 = render_typed_xychart(&build("20"), &AsciiRenderOptions::ascii())
        .expect("the second single-sample model should render");

    assert_ne!(
        x10, x20,
        "quantized authored x values must remain injective"
    );
    assert!(x10.contains("x(bytes=2)=\"10\" value=50"), "{x10}");
    assert!(x20.contains("x(bytes=2)=\"20\" value=50"), "{x20}");
}