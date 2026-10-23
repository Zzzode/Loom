fn xychart_overwide_band_category_discloses_the_complete_axis_domain() {
    let render = |unused_category: &str| {
        let model = typed_xychart_model(
            "vertical",
            XyChartAxisRenderModel::Band {
                title: String::new(),
                categories: vec!["A".to_string(), unused_category.to_string()],
            },
            0.0,
            10.0,
            vec![
                XyChartPlotRenderModel {
                    plot_type: XyChartPlotType::Line,
                    title: None,
                    values: vec![5.0],
                    data: vec![("A".to_string(), Some(5.0))],
                    point_labels: Vec::new(),
                },
                XyChartPlotRenderModel {
                    plot_type: XyChartPlotType::Line,
                    title: None,
                    values: Vec::new(),
                    data: Vec::new(),
                    point_labels: Vec::new(),
                },
            ],
        );
        let options = AsciiRenderOptions::ascii().with_xychart_category_band_width(3);

        render_typed_xychart(&model, &options)
            .expect("overwide Band categories should render through exact disclosure")
    };

    let alphabetic = render("abcdef");
    let alternate = render("abcxyz");

    assert!(
        alphabetic.contains(r#"xDomain: band categories=[bytes=1="A", bytes=6="abcdef"]"#),
        "{alphabetic}"
    );
    assert!(
        alternate.contains(r#"xDomain: band categories=[bytes=1="A", bytes=6="abcxyz"]"#),
        "{alternate}"
    );
    assert_ne!(alphabetic, alternate);
}