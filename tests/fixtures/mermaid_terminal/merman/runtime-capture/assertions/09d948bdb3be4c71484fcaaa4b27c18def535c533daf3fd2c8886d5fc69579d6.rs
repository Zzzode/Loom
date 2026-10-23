fn xychart_blank_authored_x_values_preserve_utf8_identity() {
    let build = |x: &str| {
        typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Linear {
                title: String::new(),
                min: Some(0.0),
                max: Some(1.0),
            },
            0.0,
            1.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![1.0],
                data: vec![(x.to_string(), Some(1.0))],
                point_labels: Vec::new(),
            }],
        )
    };

    let empty = render_typed_xychart(&build(""), &AsciiRenderOptions::ascii())
        .expect("the empty authored x value should render");
    let blank = render_typed_xychart(&build(" "), &AsciiRenderOptions::ascii())
        .expect("the blank authored x value should render");

    assert_ne!(empty, blank, "authored x bytes must remain injective");
    assert!(empty.contains("x(bytes=0)=\"\" value=1"), "{empty}");
    assert!(blank.contains("x(bytes=1)=\" \" value=1"), "{blank}");
}