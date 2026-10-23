fn xychart_color_truecolor_emits_axis_text_and_series_roles_without_changing_plain_text() {
    let theme = AsciiColorTheme::default_light()
        .with_role(AsciiColorRole::Text, AsciiRgb::new(1, 1, 1))
        .with_role(AsciiColorRole::ChartAxis, AsciiRgb::new(2, 2, 2))
        .with_role(AsciiColorRole::ChartSeries(0), AsciiRgb::new(3, 3, 3));
    let options = AsciiRenderOptions::ascii()
        .with_color_mode(AsciiColorMode::TrueColor)
        .with_color_theme(theme);

    let rendered = render_xychart(
        r#"xychart
title "Sales"
x-axis "Month" [Jan, Feb, Mar]
y-axis "Revenue" 0 --> 10
bar [2, 5, 8]
"#,
        &options,
    )
    .expect("xychart should render");

    assert_eq!(
        strip_ansi(&rendered),
        concat!(
            "titleDisplay: chart(bytes=5)=\"Sales\"\n",
            "titleDisplay: yAxis(bytes=7)=\"Revenue\"\n",
            "values: # series=0 type=bar title=none samples=[{index=0 x(bytes=3)=\"Jan\" value=2 pointLabel=none clipped=false}, {index=1 x(bytes=3)=\"Feb\" value=5 pointLabel=none clipped=false}, {index=2 x(bytes=3)=\"Mar\" value=8 pointLabel=none clipped=false}] orphanPointLabels=[]\n",
            "10 +\n",
            " 8 +        ###\n",
            " 6 +    ### ###\n",
            " 4 +    ### ###\n",
            " 2 +### ### ###\n",
            " 0 +-+---+---+-\n",
            "    Jan Feb Mar\n",
            "titleDisplay: xAxis(bytes=5)=\"Month\"\n",
        )
    );
    for expected_code in [
        "\u{1b}[38;2;1;1;1m",
        "\u{1b}[38;2;2;2;2m",
        "\u{1b}[38;2;3;3;3m",
    ] {
        assert!(
            rendered.contains(expected_code),
            "missing {expected_code:?} in {rendered:?}"
        );
    }
}