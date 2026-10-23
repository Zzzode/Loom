fn xychart_local_semantic_fixture_covers_horizontal_mixed_plot_with_cjk_labels() {
    let input = read_local_semantic_fixture("xychart/horizontal_mixed_cjk.mmd");

    let rendered = render_xychart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic xychart fixture should render");

    for expected in ["营收", "北区", "南区", "东区", "分数", "Bar 1", "Line 1"] {
        assert!(
            rendered.contains(expected),
            "local semantic xychart fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        first_line_index_containing(&rendered, r#"titleDisplay: chart(bytes=6)="营收""#,)
            < first_line_index_containing(&rendered, r#"titleDisplay: yAxis(bytes=6)="分数""#,),
        "title should render above the axis title:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .find(|line| line.contains("Bar 1") && line.contains("Line 1"))
            .is_some_and(|line| line.find("Bar 1") < line.find("Line 1")),
        "legend should preserve series order on the same row:\n{rendered}"
    );
    let category_row = |category: &str| {
        rendered
            .lines()
            .position(|line| line.starts_with(category))
            .unwrap_or_else(|| panic!("missing category row {category:?}:\n{rendered}"))
    };
    assert!(
        category_row("北区") < category_row("南区") && category_row("南区") < category_row("东区"),
        "CJK category labels should keep their row order:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 6,
        "local semantic xychart fixture should produce a multi-line layout:\n{rendered}"
    );
}