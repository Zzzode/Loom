fn xychart_color_html_wraps_bar_and_line_series_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::Text, AsciiRgb::from_hex24(0x101010))
        .with_role(AsciiColorRole::ChartAxis, AsciiRgb::from_hex24(0x202020))
        .with_role(
            AsciiColorRole::ChartSeries(0),
            AsciiRgb::from_hex24(0x303030),
        )
        .with_role(
            AsciiColorRole::ChartSeries(1),
            AsciiRgb::from_hex24(0x404040),
        );
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::Html)
        .with_color_theme(theme);

    let rendered = render_xychart(
        r#"xychart
x-axis [A, B]
y-axis 0 --> 10
bar [2, 8]
line [8, 2]
"#,
        &options,
    )
    .expect("mixed xychart should render");

    assert_eq!(
        strip_html_spans(&rendered),
        concat!(
            "# Bar 1  * Line 1\n",
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]\n",
            "values: # series=0 type=bar title=none samples=[{index=0 x(bytes=1)=\"A\" value=2 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=8 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "values: * series=1 type=line title=none samples=[{index=0 x(bytes=1)=\"A\" value=8 pointLabel=none clipped=false}, {index=1 x(bytes=1)=\"B\" value=2 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "10 +\n",
            " 8 + *-+###\n",
            " 6 +   |###\n",
            " 4 +   |###\n",
            " 2 +###+-*#\n",
            " 0 +-+---+-\n",
            "     A   B\n",
        )
    );
    for expected_fragment in [
        "<span style=\"color:#303030\">#</span>",
        "<span style=\"color:#404040\">*</span>",
        "<span style=\"color:#202020\">+</span>",
        "<span style=\"color:#303030\">###</span>",
        "<span style=\"color:#404040\">*-+</span>",
    ] {
        assert!(
            rendered.contains(expected_fragment),
            "missing {expected_fragment:?} in {rendered:?}"
        );
    }
}