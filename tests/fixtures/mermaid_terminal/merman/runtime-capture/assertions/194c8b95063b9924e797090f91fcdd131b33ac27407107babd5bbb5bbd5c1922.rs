fn nested_empty_subgraph_is_the_parent_endpoint_rank_anchor() {
    let rendered = render_flowchart(concat!(
        "flowchart TD\n",
        "subgraph Outer\n",
        "  subgraph Inner\n",
        "  end\n",
        "end\n",
        "Outer -->|exit| Done\n",
    ));

    for title in ["Outer", "Inner", "Done", "exit"] {
        assert!(rendered.contains(title), "missing {title:?}:\n{rendered}");
    }
    assert_eq!(rendered.matches("Outer").count(), 1, "{rendered}");
    assert_eq!(rendered.matches("Inner").count(), 1, "{rendered}");
}