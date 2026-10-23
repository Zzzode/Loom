fn xychart_parser_horizontal_bar_shows_data_labels_when_enabled() {
    let rendered = render_xychart(
        r#"%%{init: {"xyChart": {"showDataLabel": true}}}%%
xychart horizontal
x-axis [A, B]
y-axis 0 --> 10
bar [4, 8]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal xychart with data labels should render");

    assert_eq!(
        rendered,
        concat!(
            "A +###4\n",
            "B +#######8\n",
            "  ++--------+\n",
            "   0       10\n",
        )
    );
}