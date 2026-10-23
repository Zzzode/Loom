fn class_parser_parallel_self_relations_preserve_unlabelled_source_markers() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "class Node\n",
            "Node <|--|> Node : first\n",
            "Node <|--|> Node",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("parallel two-sided self class relations should render");

    assert_eq!(
        rendered.matches("| Node |").count(),
        1,
        "parallel self relations must share one class box:\n{rendered}"
    );
    for marker in ['^', 'v'] {
        assert!(
            rendered.matches(marker).count() >= 2,
            "each parallel self relation must preserve its {marker:?} marker:\n{rendered}"
        );
    }
}