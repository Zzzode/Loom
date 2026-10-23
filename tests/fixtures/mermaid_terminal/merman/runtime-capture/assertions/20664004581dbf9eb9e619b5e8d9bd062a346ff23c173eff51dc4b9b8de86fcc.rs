fn xychart_direct_model_title_frames_distinguish_lf_from_literal_escape() {
    let render = |chart_title: &str| {
        let mut model = typed_xychart_model(
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
                point_labels: Vec::new(),
            }],
        );
        model.title = Some(chart_title.to_string());
        model.display.show_title = true;
        model.display.x_axis.show_title = false;
        model.display.y_axis.show_title = false;
        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("owned title fixture should render")
    };

    let structural_lf = render("A\nB");
    let authored_escape = render(r"A\nB");

    assert_ne!(
        structural_lf, authored_escape,
        "a structural line break must not collapse with authored escape text"
    );
    assert!(
        structural_lf.contains(r#"titleDisplay: chart(bytes=3)="A\nB""#),
        "structural LF must retain its source byte length:\n{structural_lf}"
    );
    assert!(
        authored_escape.contains(r#"titleDisplay: chart(bytes=4)="A\\nB""#),
        "literal escape text must remain distinguishable from LF:\n{authored_escape}"
    );
}