fn xychart_horizontal_unequal_linear_series_share_exact_sample_labels() {
    let mut model = typed_xychart_model(
        "horizontal",
        XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: Some(0.0),
            max: Some(10.0),
        },
        0.0,
        10.0,
        vec![
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: Some("Sparse".to_string()),
                values: vec![1.0, 9.0],
                data: vec![("0".to_string(), Some(1.0)), ("10".to_string(), Some(9.0))],
                point_labels: Vec::new(),
            },
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: Some("Dense".to_string()),
                values: vec![2.0, 4.0, 6.0, 8.0],
                data: vec![
                    ("0".to_string(), Some(2.0)),
                    ("3".to_string(), Some(4.0)),
                    ("7".to_string(), Some(6.0)),
                    ("10".to_string(), Some(8.0)),
                ],
                point_labels: Vec::new(),
            },
        ],
    );
    model.display.x_axis = XyChartAxisDisplayPolicy::default();

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("unequal linear series should share an exact horizontal axis");
    let first_tokens = rendered
        .lines()
        .filter_map(|line| line.split_whitespace().next())
        .collect::<Vec<_>>();

    for expected in ["0", "3", "7", "10"] {
        assert!(
            first_tokens.contains(&expected),
            "missing authored x={expected} row label:\n{rendered}"
        );
    }
    for expected in [
        "title(bytes=6)=\"Sparse\" samples=[{index=0 x(bytes=1)=\"0\" value=1",
        "title(bytes=5)=\"Dense\" samples=[{index=0 x(bytes=1)=\"0\" value=2",
        "index=1 x(bytes=1)=\"3\" value=4",
        "index=2 x(bytes=1)=\"7\" value=6",
        "index=3 x(bytes=2)=\"10\" value=8",
    ] {
        assert!(rendered.contains(expected), "{rendered}");
    }
}