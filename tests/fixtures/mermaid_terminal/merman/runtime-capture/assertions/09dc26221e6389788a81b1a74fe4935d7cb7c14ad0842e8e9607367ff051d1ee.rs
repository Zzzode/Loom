fn xychart_missing_samples_break_paths_and_remain_explicit() {
    for orientation in ["vertical", "horizontal"] {
        let model = typed_xychart_model(
            orientation,
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string(), "B".to_string(), "C".to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![2.0, 8.0],
                data: vec![
                    ("A".to_string(), Some(2.0)),
                    ("B".to_string(), None),
                    ("C".to_string(), Some(8.0)),
                ],
                point_labels: Vec::new(),
            }],
        );

        let rendered = render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .unwrap_or_else(|error| panic!("{orientation} sparse line should render: {error}"));
        assert!(
            rendered.contains("x(bytes=1)=\"B\" value=none"),
            "{orientation} sparse line must disclose the missing sample:\n{rendered}"
        );
        let plot = rendered
            .lines()
            .filter(|line| !line.starts_with("values:"))
            .collect::<Vec<_>>()
            .join("\n");
        assert_eq!(
            plot.matches('*').count(),
            2,
            "{orientation} sparse line should retain both defined points:\n{rendered}"
        );
        assert!(
            !plot.chars().any(|ch| matches!(ch, '-' | '|' | '+')),
            "{orientation} sparse line must not bridge across a missing sample:\n{rendered}"
        );
    }
}