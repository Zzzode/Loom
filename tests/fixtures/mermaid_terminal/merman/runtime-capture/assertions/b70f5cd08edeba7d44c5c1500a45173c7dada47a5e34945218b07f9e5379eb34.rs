fn xychart_horizontal_grouped_bars_get_independent_rows() {
    let model = typed_xychart_model(
        "horizontal",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string(), "B".to_string()],
        },
        0.0,
        10.0,
        vec![
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: Some("first".to_string()),
                values: vec![2.0, 8.0],
                data: vec![("A".to_string(), Some(2.0)), ("B".to_string(), Some(8.0))],
                point_labels: Vec::new(),
            },
            XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: Some("second".to_string()),
                values: vec![7.0, 3.0],
                data: vec![("A".to_string(), Some(7.0)), ("B".to_string(), Some(3.0))],
                point_labels: Vec::new(),
            },
        ],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
        .expect("grouped horizontal bars should render");
    let bar_rows = rendered
        .lines()
        .filter(|line| !line.is_empty() && line.chars().all(|ch| ch == '#'))
        .count();

    assert_eq!(
        bar_rows, 4,
        "two bar series across two categories need four recoverable lanes:\n{rendered}"
    );
}