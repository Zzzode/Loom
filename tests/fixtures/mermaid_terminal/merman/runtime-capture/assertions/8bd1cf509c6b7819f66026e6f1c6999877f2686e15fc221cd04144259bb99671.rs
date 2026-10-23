fn flowchart_style_color_html_maps_linkstyle_edge_and_label_foreground_without_plain_text_changes()
{
    let input = concat!(
        "flowchart LR\n",
        "  A[Alpha] -->|go| B[Beta]\n",
        "  linkStyle 0 stroke:#123456,color:#654321\n",
    );
    let options = AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html);

    let rendered = render_flowchart(input, &options).unwrap();
    let plain = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(strip_html_spans(&rendered), plain);
    assert!(
        rendered.contains("<span style=\"color:#123456\">--</span>")
            && rendered.contains("<span style=\"color:#123456\">--&gt;</span>"),
        "missing styled edge line or arrow in {rendered:?}"
    );
    assert!(
        rendered.contains("<span style=\"color:#654321\">go</span>"),
        "missing styled edge label in {rendered:?}"
    );
}