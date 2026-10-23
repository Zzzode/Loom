fn flowchart_local_semantic_fixture_covers_disconnected_subgraphs() {
    let input = local_semantic_input("flowchart/disconnected_subgraphs.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic disconnected-subgraphs flowchart fixture should render");

    for expected in [
        "Today",
        "Next Wave",
        "Today AI",
        "Today Markdown",
        "Today Reads",
        "Next AI",
        "Next Widget",
        "Next Acts",
    ] {
        assert!(
            rendered.contains(expected),
            "disconnected-subgraphs fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    assert_rectangular_char_grid(&rendered);

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert_ne!(
        line_index("Today"),
        line_index("Next Wave"),
        "disconnected subgraphs should remain separate visible groups:\n{rendered}"
    );
    assert!(
        line_index("Today AI") < line_index("Today Markdown")
            && line_index("Today Markdown") < line_index("Today Reads"),
        "an implicit isolated group under LR should use Mermaid's vertical default:\n{rendered}"
    );
    assert!(
        line_index("Next AI") < line_index("Next Widget")
            && line_index("Next Widget") < line_index("Next Acts"),
        "each implicit isolated group should use Mermaid's vertical default:\n{rendered}"
    );
}