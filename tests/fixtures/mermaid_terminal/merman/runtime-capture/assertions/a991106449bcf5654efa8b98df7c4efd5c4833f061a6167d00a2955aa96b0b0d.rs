fn xychart_point_labels_preserve_authored_whitespace_and_presence() {
    let render = |point_labels: Vec<&str>| {
        let model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Line,
                title: None,
                values: vec![5.0],
                data: vec![("A".to_string(), Some(5.0))],
                point_labels: point_labels.into_iter().map(str::to_string).collect(),
            }],
        );

        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("authored point-label whitespace should render")
    };

    let leading = render(vec![" peak"]);
    let trailing = render(vec!["peak "]);
    let whitespace = render(vec![" "]);
    let absent = render(Vec::new());
    let orphan_whitespace = render(vec!["peak", " "]);

    assert!(
        leading.contains(r#"pointLabel(bytes=5)=" peak""#),
        "{leading}"
    );
    assert!(
        trailing.contains(r#"pointLabel(bytes=5)="peak ""#),
        "{trailing}"
    );
    assert!(
        whitespace.contains(r#"pointLabel(bytes=1)=" ""#),
        "{whitespace}"
    );
    assert!(
        orphan_whitespace.contains(r#"orphanPointLabels=[bytes=1=" "]"#),
        "{orphan_whitespace}"
    );
    assert_ne!(leading, trailing);
    assert_ne!(whitespace, absent);
}