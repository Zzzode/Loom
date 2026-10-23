fn xychart_tiny_axis_ticks_remain_distinct_and_scale_aware() {
    let rendered = render_xychart(
        r#"xychart
x-axis [A, B]
y-axis 0.001 --> 0.005
line [0.001, 0.005]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("tiny XYChart range should render");

    assert!(
        rendered.contains("0.005"),
        "missing exact upper tick:\n{rendered}"
    );
    assert!(
        rendered.contains("0.001"),
        "missing exact lower tick:\n{rendered}"
    );
    let visible_ticks = rendered
        .lines()
        .take(5)
        .filter_map(|line| line.split_whitespace().next())
        .collect::<Vec<_>>();
    assert_eq!(
        visible_ticks.len(),
        5,
        "compact value axis should retain five tick rows:\n{rendered}"
    );
    assert!(
        visible_ticks.windows(2).all(|pair| pair[0] != pair[1]),
        "scale-aware formatting must not collapse adjacent ticks:\n{rendered}"
    );
}