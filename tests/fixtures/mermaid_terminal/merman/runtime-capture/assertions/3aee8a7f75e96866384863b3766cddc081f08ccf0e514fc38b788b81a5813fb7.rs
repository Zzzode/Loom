fn xychart_multisample_quantized_x_remains_exact_across_models() {
    let build = |first_x: &str| {
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
                values: vec![20.0, 80.0],
                data: vec![
                    (first_x.to_string(), Some(20.0)),
                    ("90".to_string(), Some(80.0)),
                ],
                point_labels: Vec::new(),
            }],
        )
    };

    let x10 = render_typed_xychart(&build("10"), &AsciiRenderOptions::ascii())
        .expect("the first multisample model should render");
    let x11 = render_typed_xychart(&build("11"), &AsciiRenderOptions::ascii())
        .expect("the second multisample model should render");

    assert_ne!(x10, x11, "quantized multisample x values must be injective");
    assert!(x10.contains("x(bytes=2)=\"10\" value=20"), "{x10}");
    assert!(x11.contains("x(bytes=2)=\"11\" value=20"), "{x11}");
}