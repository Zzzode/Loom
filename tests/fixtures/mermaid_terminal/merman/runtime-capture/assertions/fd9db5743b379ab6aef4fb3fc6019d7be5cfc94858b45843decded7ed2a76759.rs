fn xychart_quantized_line_collision_triggers_exact_disclosure() {
    let model = typed_xychart_model(
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
            values: vec![5.0, 5.0],
            data: vec![("0".to_string(), Some(5.0)), ("0.1".to_string(), Some(5.0))],
            point_labels: Vec::new(),
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("quantized line collision should render through disclosure");

    assert!(
        rendered.contains("index=0 x(bytes=1)=\"0\" value=5")
            && rendered.contains("index=1 x(bytes=3)=\"0.1\" value=5"),
        "colliding typed coordinates must remain exact:\n{rendered}"
    );
    let plotted_points = rendered
        .lines()
        .filter(|line| !line.starts_with("values:"))
        .map(|line| line.matches('*').count())
        .sum::<usize>();
    assert_eq!(
        plotted_points, 1,
        "the probe should exercise one quantized terminal cell:\n{rendered}"
    );
}