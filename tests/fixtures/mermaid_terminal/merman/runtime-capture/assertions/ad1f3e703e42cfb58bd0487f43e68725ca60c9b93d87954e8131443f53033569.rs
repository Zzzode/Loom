fn flowchart_local_semantic_fixture_covers_ampersand_fanin_and_fanout() {
    let input = local_semantic_input("flowchart/ampersand_fanin_fanout.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic ampersand flowchart fixture should render");

    for expected in [
        "SourceA", "SourceB", "Merge", "Fanout", "TargetA", "TargetB",
    ] {
        assert!(
            rendered.contains(expected),
            "ampersand fan-in/fan-out fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert_eq!(
        line_index("SourceA"),
        line_index("SourceB"),
        "fan-in sources should stay on the same semantic rank:\n{rendered}"
    );
    assert!(
        line_index("SourceA") < line_index("Merge"),
        "fan-in target should render after both source nodes:\n{rendered}"
    );
    assert!(
        line_index("Fanout") < line_index("TargetA"),
        "fan-out source should render before target nodes:\n{rendered}"
    );
    assert_eq!(
        line_index("TargetA"),
        line_index("TargetB"),
        "fan-out targets should stay on the same semantic rank:\n{rendered}"
    );
}