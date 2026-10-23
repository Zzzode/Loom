fn flowchart_process_and_decorated_process_shapes_keep_distinct_terminal_geometry() {
    let render = |shape: &str| {
        render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"P\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap()
    };

    let rect = render("process");
    let stacked = render("st-rect");
    let lined = render("lin-rect");
    let tagged = render("tag-rect");

    for rendered in [&rect, &stacked, &lined, &tagged] {
        assert!(rendered.contains('P'));
    }
    assert_ne!(stacked, rect, "stacked process must not collapse to rect");
    assert_ne!(lined, rect, "lined process must not collapse to rect");
    assert_ne!(tagged, rect, "tagged process must not collapse to rect");
    assert_ne!(stacked, lined);
    assert_ne!(stacked, tagged);
    assert_ne!(lined, tagged);
}