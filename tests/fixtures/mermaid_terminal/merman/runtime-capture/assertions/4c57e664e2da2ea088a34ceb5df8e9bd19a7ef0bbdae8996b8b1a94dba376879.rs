fn xychart_dense_linear_bar_overlap_triggers_exact_disclosure() {
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
            plot_type: XyChartPlotType::Bar,
            title: None,
            values: vec![5.0, 8.0],
            data: vec![("0".to_string(), Some(5.0)), ("20".to_string(), Some(8.0))],
            point_labels: Vec::new(),
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("dense linear bars should render through disclosure");

    assert!(
        rendered.contains("index=0 x(bytes=1)=\"0\" value=5")
            && rendered.contains("index=1 x(bytes=2)=\"20\" value=8"),
        "overlapping bar identities must remain exact:\n{rendered}"
    );
}