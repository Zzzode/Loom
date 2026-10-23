fn xychart_parser_vertical_bar_shows_data_labels_when_enabled() {
    let rendered = render_xychart(
        r#"%%{init: {"xyChart": {"showDataLabel": true}}}%%
xychart
x-axis [A, B]
y-axis 0 --> 10
bar [4, 8]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("vertical xychart with data labels should render");

    assert_eq!(
        rendered,
        concat!(
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]\n",
            "10 +\n",
            " 8 +     8\n",
            " 6 +    ###\n",
            " 4 + 4  ###\n",
            " 2 +### ###\n",
            " 0 +-+---+-\n",
            "     A   B\n",
        )
    );
}