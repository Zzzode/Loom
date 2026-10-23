fn flowchart_local_semantic_fixture_covers_multiline_edge_labels() {
    let input = local_semantic_input("flowchart/multiline_edge_label.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic multiline-edge fixture should render");

    for expected in ["A", "B", "north", "south"] {
        assert!(
            rendered.contains(expected),
            "multiline edge-label fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("<br>"),
        "multiline edge-label fixture should not leak HTML break markup:\n{rendered}"
    );

    let north = first_line_index_containing(&rendered, "north");
    let south = first_line_index_containing(&rendered, "south");
    assert_eq!(
        south,
        north + 1,
        "multiline edge-label fixture should stack label rows in order:\n{rendered}"
    );
}