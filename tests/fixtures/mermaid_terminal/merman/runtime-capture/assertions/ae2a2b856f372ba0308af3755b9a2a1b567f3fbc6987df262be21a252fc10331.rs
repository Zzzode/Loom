fn class_parser_horizontal_parallel_self_relations_share_one_box() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction LR\n",
            "class Node\n",
            "Node --> Node : next\n",
            "Node ..> Node : loads",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal parallel self relations should render");

    assert_eq!(
        rendered.matches("| Node |").count(),
        1,
        "parallel self relations must share one class box:\n{rendered}"
    );
    for label in ["next", "loads"] {
        assert!(
            rendered.contains(label),
            "parallel self relation must preserve {label:?}:\n{rendered}"
        );
    }
    assert!(
        rendered.matches('v').count() >= 2,
        "each self relation must retain its terminal marker:\n{rendered}"
    );
}