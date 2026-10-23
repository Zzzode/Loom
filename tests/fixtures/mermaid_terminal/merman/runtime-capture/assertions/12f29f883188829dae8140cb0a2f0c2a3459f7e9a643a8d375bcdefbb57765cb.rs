fn xychart_parser_vertical_bar_renders_unicode_chart_chars() {
    let rendered = render_xychart(
        r#"xychart
x-axis [A, B]
y-axis 0 --> 5
bar [2, 5]
"#,
        &AsciiRenderOptions::unicode(),
    )
    .expect("unicode xychart should render");

    assert_eq!(
        rendered,
        concat!(
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]\n",
            "5 ┤    ███\n",
            "4 ┤    ███\n",
            "3 ┤    ███\n",
            "2 ┤███ ███\n",
            "1 ┤███ ███\n",
            "0 ┼─┬───┬─\n",
            "    A   B\n",
        )
    );
}