fn flowchart_parser_duplicate_subgraph_ids_keep_first_presentation_and_all_members() {
    let rendered = render_flowchart(
        "flowchart TB\nsubgraph X[First]\nA\nend\nsubgraph X[Second]\nB\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("duplicate subgraph declarations should merge into one rendered group");

    assert_eq!(
        rendered.matches("First").count(),
        1,
        "the first declaration should own the rendered title:\n{rendered}"
    );
    assert!(
        !rendered.contains("Second"),
        "later presentation metadata must not replace the first declaration:\n{rendered}"
    );
    for member in [" A ", " B "] {
        assert_eq!(
            rendered.matches(member).count(),
            1,
            "all declarations must contribute their members exactly once:\n{rendered}"
        );
    }
}