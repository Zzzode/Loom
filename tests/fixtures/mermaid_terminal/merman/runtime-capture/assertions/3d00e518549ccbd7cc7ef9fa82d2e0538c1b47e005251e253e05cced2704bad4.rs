fn xychart_empty_projection_enforces_authored_grapheme_budget() {
    let grapheme = "👩‍💻";
    let model = XyChartDiagramRenderModel {
        orientation: "vertical".to_string(),
        title: Some(grapheme.to_string()),
        acc_title: None,
        acc_descr: None,
        x_axis: XyChartAxisRenderModel::Band {
            title: String::new(),
            categories: Vec::new(),
        },
        y_axis: XyChartAxisRenderModel::Linear {
            title: String::new(),
            min: None,
            max: None,
        },
        plots: Vec::new(),
        display: XyChartDisplayPolicy::default(),
    };
    let exact = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxGraphemeBytes, grapheme.len())
        .expect("valid exact grapheme-byte limit");

    assert!(
        render_typed_xychart_with_resources(&model, &AsciiRenderOptions::ascii(), exact)
            .expect("exact empty-chart grapheme budget should render")
            .contains(grapheme)
    );

    let below = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxGraphemeBytes, grapheme.len() - 1)
        .expect("valid N-1 grapheme-byte limit");
    let error = render_typed_xychart_with_resources(&model, &AsciiRenderOptions::ascii(), below)
        .expect_err("N-1 empty-chart grapheme budget should reject authored text");
    let AsciiError::ResourceLimitExceeded(details) = error else {
        panic!("expected grapheme resource error, got {error:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxGraphemeBytes);
    assert_eq!(details.actual, grapheme.len());
    assert_eq!(details.max, grapheme.len() - 1);
}