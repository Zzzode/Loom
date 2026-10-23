fn xychart_plot_area_options_scale_vertical_chart() {
    let options = AsciiRenderOptions::ascii()
        .with_xychart_vertical_plot_height(4)
        .with_xychart_category_band_width(4);

    let rendered = render_xychart(
        r#"xychart
x-axis [Jan, Feb]
y-axis 0 --> 8
bar [4, 8]
"#,
        &options,
    )
    .expect("xychart should render with custom vertical plot area");

    assert_eq!(
        rendered,
        concat!(
            "xDomain: band categories=[bytes=3=\"Jan\", bytes=3=\"Feb\"]\n",
            "8 +     ####\n",
            "6 +     ####\n",
            "4 +#### ####\n",
            "2 +#### ####\n",
            "0 +--+----+-\n",
            "   Jan  Feb\n",
        )
    );
}