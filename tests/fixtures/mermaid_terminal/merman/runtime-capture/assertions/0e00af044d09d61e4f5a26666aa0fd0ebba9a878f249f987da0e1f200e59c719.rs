fn xychart_plot_area_respects_resource_limits() {
    let options = AsciiRenderOptions::ascii();

    let err = render_xychart_with_grid_limit(
        r#"xychart
x-axis [A, B]
y-axis 0 --> 10
bar [4, 8]
"#,
        &options,
        3,
    )
    .expect_err("xychart plot area should respect max_ascii_grid_cells");

    let AsciiError::ResourceLimitExceeded(details) = err else {
        panic!("expected a typed resource-limit error, got {err:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxGridCells);
    assert_eq!(details.actual, 35);
    assert_eq!(details.max, 3);
}