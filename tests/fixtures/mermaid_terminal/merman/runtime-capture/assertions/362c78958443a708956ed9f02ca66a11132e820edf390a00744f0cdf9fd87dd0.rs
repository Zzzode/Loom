fn flowchart_local_semantic_fixture_covers_back_edge_labels() {
    let input = local_semantic_input("flowchart/back_edge_labels.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic back-edge label fixture should render");

    for expected in ["A", "B", "C", "back to top", "back to middle"] {
        assert!(
            rendered.contains(expected),
            "back-edge label fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert!(
        line_index("A") < line_index("B"),
        "root TD flow should keep A above B:\n{rendered}"
    );
    assert!(
        line_index("B") < line_index("C"),
        "root TD flow should keep B above C:\n{rendered}"
    );
    assert_ne!(
        line_index("back to top"),
        line_index("back to middle"),
        "separate back-edge labels should not overwrite each other:\n{rendered}"
    );
    let marker_count = rendered
        .chars()
        .filter(|ch| matches!(ch, '^' | '>' | 'v' | '<'))
        .count();
    assert_eq!(
        marker_count, 4,
        "all four directed edges should retain an independently visible target marker:\n{rendered}"
    );
}