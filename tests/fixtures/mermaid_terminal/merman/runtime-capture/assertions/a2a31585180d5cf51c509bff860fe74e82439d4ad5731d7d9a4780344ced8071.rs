fn xychart_duplicate_categories_keep_source_order_and_disclosure_identity() {
    let model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["B".to_string(), "A".to_string(), "A".to_string()],
        },
        0.0,
        3.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: None,
            values: vec![0.0, 1.0, 2.0],
            data: vec![
                ("B".to_string(), Some(0.0)),
                ("A".to_string(), Some(1.0)),
                ("A".to_string(), Some(2.0)),
            ],
            point_labels: Vec::new(),
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("duplicate categories should render deterministically");

    assert!(
        rendered.contains("index=0 x(bytes=1)=\"B\" value=0")
            && rendered.contains("index=1 x(bytes=1)=\"A\" value=1")
            && rendered.contains("index=2 x(bytes=1)=\"A\" value=2"),
        "duplicate categories need stable occurrence identities:\n{rendered}"
    );
    let plotted_points = rendered
        .lines()
        .filter(|line| !line.starts_with("values:"))
        .map(|line| line.matches('*').count())
        .sum::<usize>();
    assert_eq!(
        plotted_points, 3,
        "duplicate category samples must retain separate source-order slots:\n{rendered}"
    );
}