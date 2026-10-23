fn xychart_band_domain_discloses_terminal_normalization_identity() {
    let render = |category: &str| {
        let model = typed_xychart_model(
            "horizontal",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec![category.to_string()],
            },
            0.0,
            10.0,
            vec![XyChartPlotRenderModel {
                plot_type: XyChartPlotType::Bar,
                title: None,
                values: vec![5.0],
                data: vec![(category.to_string(), Some(5.0))],
                point_labels: Vec::new(),
            }],
        );

        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("Band category normalization should remain injective")
    };

    let control = render("\u{1b}");
    let authored_escape = render(r"\u{1B}");

    assert!(
        control.contains(r#"xDomain: band categories=[bytes=1="\u{1B}"]"#),
        "the normalized control must retain its authored byte identity:\n{control}"
    );
    assert!(
        authored_escape.contains(r#"xDomain: band categories=[bytes=6="\\u{1B}"]"#),
        "hidden Band geometry must frame the unchanged printable value:\n{authored_escape}"
    );
    assert_ne!(control, authored_escape);
}