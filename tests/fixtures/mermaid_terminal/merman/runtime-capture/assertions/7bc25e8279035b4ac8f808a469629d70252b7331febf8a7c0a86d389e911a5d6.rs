fn xychart_plot_area_options_scale_horizontal_chart() {
    let options = AsciiRenderOptions::ascii().with_xychart_horizontal_plot_width(5);

    let rendered = render_xychart(
        r#"xychart horizontal
x-axis [A, B]
y-axis 0 --> 10
bar [4, 8]
"#,
        &options,
    )
    .expect("xychart should render with custom horizontal plot area");

    assert_eq!(
        rendered,
        concat!("A +##\n", "B +####\n", "  ++---+\n", "   0  10\n",)
    );
}