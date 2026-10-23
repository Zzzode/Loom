fn standalone_empty_subgraph_renders_a_real_group_perimeter() {
    let rendered = render_flowchart(concat!(
        "flowchart TD\n",
        "subgraph Empty[Empty Group]\n",
        "end\n",
    ));

    assert_eq!(rendered.matches("Empty Group").count(), 1, "{rendered}");
    assert!(
        rendered.contains('+'),
        "empty group should draw corners:\n{rendered}"
    );
    assert!(
        rendered.contains('|'),
        "empty group should draw sides:\n{rendered}"
    );
}