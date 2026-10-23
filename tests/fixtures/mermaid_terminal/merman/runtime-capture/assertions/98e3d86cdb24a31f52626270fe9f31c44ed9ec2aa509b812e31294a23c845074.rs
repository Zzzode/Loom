fn flowchart_duplicate_subgraph_declarations_deduplicate_repeated_members() {
    let rendered = render_flowchart(
        "flowchart TB\nsubgraph X[First]\nA\nend\nsubgraph X[Second]\nA\nB\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("duplicate subgraph declarations should preserve set-like membership");

    assert_eq!(rendered.matches(" A ").count(), 1, "{rendered}");
    assert_eq!(rendered.matches(" B ").count(), 1, "{rendered}");
}