fn flowchart_style_color_html_maps_node_fill_background_without_plain_text_changes() {
    let input = concat!(
        "flowchart LR\n",
        "  A[Alpha]:::hot\n",
        "  classDef hot color:#112233,stroke:#445566,fill:#ffeecc\n",
    );
    let options = AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html);

    let rendered = render_flowchart(input, &options).unwrap();
    let plain = render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(strip_html_spans(&rendered), plain);
    assert!(
        rendered.contains("background-color:#ffeecc"),
        "missing node fill background in {rendered:?}"
    );
}