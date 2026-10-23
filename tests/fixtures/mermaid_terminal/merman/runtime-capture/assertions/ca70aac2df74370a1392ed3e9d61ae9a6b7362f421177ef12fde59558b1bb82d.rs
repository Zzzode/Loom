fn xychart_direct_model_title_frames_preserve_trim_and_esc() {
    let render = |chart_title: &str, x_title: &str, y_title: &str| {
        let mut model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: x_title.to_string(),
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
        model.y_axis = XyChartAxisRenderModel::Linear {
            title: y_title.to_string(),
            min: Some(0.0),
            max: Some(10.0),
        };
        model.display.show_title = true;
        model.display.x_axis.show_title = true;
        model.display.y_axis.show_title = true;
        render_typed_xychart(&model, &AsciiRenderOptions::ascii())
            .expect("owned title frames should render")
    };

    let control_and_left_trim = render("\u{1b}", " X", "Y ");
    let authored_escape_and_right_trim = render("\\u{1B}", "X ", " Y");

    assert_ne!(
        control_and_left_trim, authored_escape_and_right_trim,
        "distinct authored titles must not collapse after terminal escaping"
    );
    for expected in [
        r#"titleDisplay: chart(bytes=1)="\u{1B}""#,
        r#"titleDisplay: xAxis(bytes=2)=" X""#,
        r#"titleDisplay: yAxis(bytes=2)="Y ""#,
    ] {
        assert!(
            control_and_left_trim.contains(expected),
            "missing exact owned title frame {expected:?}:\n{control_and_left_trim}"
        );
    }
    for expected in [
        r#"titleDisplay: chart(bytes=6)="\\u{1B}""#,
        r#"titleDisplay: xAxis(bytes=2)="X ""#,
        r#"titleDisplay: yAxis(bytes=2)=" Y""#,
    ] {
        assert!(
            authored_escape_and_right_trim.contains(expected),
            "missing exact authored escape frame {expected:?}:\n{authored_escape_and_right_trim}"
        );
    }
}