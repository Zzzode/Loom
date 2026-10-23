fn xychart_linear_x_lexemes_remain_injective_after_numeric_projection() {
    let build = |x: &str| {
        typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Linear {
                title: String::new(),
                min: Some(0.0),
                max: Some(100.0),
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![4.0],
                data: vec![(x.to_string(), Some(4.0))],
                point_labels: Vec::new(),
            }],
        )
    };

    let integer = render_typed_xychart(&build("0"), &AsciiRenderOptions::ascii())
        .expect("the integer x lexeme should render");
    let decimal = render_typed_xychart(&build("0.0"), &AsciiRenderOptions::ascii())
        .expect("the decimal x lexeme should render");

    assert_ne!(integer, decimal);
    assert!(integer.contains("x(bytes=1)=\"0\" value=4"), "{integer}");
    assert!(decimal.contains("x(bytes=3)=\"0.0\" value=4"), "{decimal}");
}