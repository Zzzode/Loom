fn xychart_parser_respects_title_and_axis_visibility_config() {
    let rendered = render_xychart(
        r#"%%{init: {"xyChart": {"showTitle": false, "xAxis": {"showLabel": false, "showTitle": false, "showTick": false, "showAxisLine": false}, "yAxis": {"showLabel": false, "showTitle": false, "showTick": false, "showAxisLine": false}}}}%%
xychart
title "Sales"
x-axis "Month" [A, B]
y-axis "Revenue" 0 --> 10
bar [4, 8]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("xychart with hidden titles and axes should render");

    assert!(rendered.contains("###"));
    assert!(
        rendered.contains(r#"xDomain: band categories=[bytes=1="A", bytes=1="B"]"#),
        "hidden geometry must retain its authored Band identity:\n{rendered}"
    );
    for hidden in ["Sales", "Month", "Revenue", "|", "+", "-"] {
        assert!(
            !rendered.contains(hidden),
            "hidden token {hidden:?} should not be rendered:\n{rendered}"
        );
    }
}