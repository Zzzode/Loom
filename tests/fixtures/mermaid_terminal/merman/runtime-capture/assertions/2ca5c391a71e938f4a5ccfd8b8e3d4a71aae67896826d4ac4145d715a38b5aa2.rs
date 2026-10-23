fn class_local_semantic_fixture_covers_annotation_methods() {
    let input = read_local_semantic_fixture("class/annotation_methods.mmd");
    let rendered = render_class(&input, &AsciiRenderOptions::ascii())
        .expect("class annotation and methods fixture should render");

    for expected in [
        "<<abstract>>",
        "Shape",
        "+draw() : void",
        "Circle",
        "+radius int",
        "^",
    ] {
        assert!(
            rendered.contains(expected),
            "class annotation fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    assert!(
        first_line_index_containing(&rendered, "Shape")
            < first_line_index_containing(&rendered, "Circle"),
        "inheritance should keep Shape before Circle in the routed terminal layout:\n{rendered}"
    );
}