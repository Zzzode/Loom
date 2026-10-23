fn zero_vertical_padding_reduces_only_node_height() {
    let input = "flowchart TD\n A[Write code] --> B[Open PR] --> C[Merge]\n";
    let default = render_flowchart(input, &AsciiRenderOptions::unicode()).unwrap();
    let compact_nodes =
        render_flowchart(input, &AsciiRenderOptions::unicode().with_node_padding_y(0)).unwrap();
    assert_eq!(default.lines().count(), 25);
    assert_eq!(compact_nodes.lines().count(), 19);
    assert_eq!(
        default.lines().map(|line| line.chars().count()).max(),
        compact_nodes.lines().map(|line| line.chars().count()).max()
    );
    for label in ["Write code", "Open PR", "Merge"] {
        assert_eq!(compact_nodes.matches(label).count(), 1);
    }
}