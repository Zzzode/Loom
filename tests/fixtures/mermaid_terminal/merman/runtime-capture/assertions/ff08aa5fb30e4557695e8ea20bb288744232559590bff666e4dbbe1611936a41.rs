fn xychart_linear_x_coordinates_control_terminal_point_spacing() {
    let make_model = |second_x: &str| {
        typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Linear {
                title: String::new(),
                min: Some(0.0),
                max: Some(10.0),
            },
            0.0,
            1.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![0.5, 0.5],
                data: vec![
                    ("0".to_string(), Some(0.5)),
                    (second_x.to_string(), Some(0.5)),
                ],
                point_labels: Vec::new(),
            }],
        )
    };
    let options = AsciiRenderOptions::ascii()
        .with_xychart_vertical_plot_height(3)
        .with_xychart_category_band_width(3);

    let near = render_typed_xychart(&make_model("2"), &options)
        .expect("near linear samples should render");
    let far = render_typed_xychart(&make_model("10"), &options)
        .expect("far linear samples should render");
    let near_width = near.lines().map(str::len).max().unwrap_or_default();
    let far_width = far.lines().map(str::len).max().unwrap_or_default();

    assert!(
        far_width > near_width,
        "typed x coordinates should change point spacing (near={near:?}, far={far:?})"
    );
}