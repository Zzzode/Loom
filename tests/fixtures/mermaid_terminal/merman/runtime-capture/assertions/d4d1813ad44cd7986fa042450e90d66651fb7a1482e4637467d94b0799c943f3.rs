fn xychart_parser_vertical_bar_can_place_data_labels_outside_bars() {
    let rendered = render_xychart(
        r#"%%{init: {"xyChart": {"showDataLabel": true, "showDataLabelOutsideBar": true}}}%%
xychart
x-axis [A, B]
y-axis 0 --> 10
bar [4, 8]
"#,
        &AsciiRenderOptions::ascii(),
    )
    .expect("vertical xychart with outside data labels should render");

    let mut lines = rendered.lines();
    assert_eq!(
        lines.next(),
        Some("xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]")
    );
    assert_eq!(lines.next(), Some("     4   8"));
    assert_eq!(
        rendered,
        concat!(
            "xDomain: band categories=[bytes=1=\"A\", bytes=1=\"B\"]\n",
            "     4   8\n",
            "10 +\n",
            " 8 +    ###\n",
            " 6 +    ###\n",
            " 4 +### ###\n",
            " 2 +### ###\n",
            " 0 +-+---+-\n",
            "     A   B\n",
        )
    );
}