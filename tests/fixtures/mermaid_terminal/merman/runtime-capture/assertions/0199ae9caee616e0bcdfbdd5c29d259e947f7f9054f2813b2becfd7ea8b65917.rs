fn flowchart_parser_top_down_double_point_edge_keeps_both_directions() {
    let rendered =
        render_flowchart("flowchart TD\nA <--> B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(rendered.matches('^').count(), 1, "{rendered}");
    assert_eq!(rendered.matches('v').count(), 1, "{rendered}");
}