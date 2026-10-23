fn xychart_unicode_line_topology_resolves_rounded_corners() {
    let model = typed_xychart_model(
        "vertical",
        XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: vec!["A".to_string(), "B".to_string()],
        },
        0.0,
        10.0,
        vec![XyChartPlotRenderModel {
            plot_type: XyChartPlotType::Line,
            title: None,
            values: vec![2.0, 8.0],
            data: vec![("A".to_string(), Some(2.0)), ("B".to_string(), Some(8.0))],
            point_labels: Vec::new(),
        }],
    );

    let rendered = render_typed_xychart(&model, &AsciiRenderOptions::unicode())
        .expect("Unicode line topology should render");

    for glyph in ['╭', '╯', '─', '│', '●'] {
        assert!(
            rendered.contains(glyph),
            "Unicode line topology should contain {glyph:?}:\n{rendered}"
        );
    }
}