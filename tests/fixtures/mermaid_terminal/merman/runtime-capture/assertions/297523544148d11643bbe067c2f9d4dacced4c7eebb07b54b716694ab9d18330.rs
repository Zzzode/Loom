fn class_parser_horizontal_component_draws_each_class_once() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction LR\n",
            "class A\n",
            "class B\n",
            "class C\n",
            "A <|-- B : parent\n",
            "A ..> B : depends\n",
            "B --> C : next",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal class component should render");

    for title in ["| A |", "| B |", "| C |"] {
        assert_eq!(
            rendered.matches(title).count(),
            1,
            "a horizontal component must place {title:?} exactly once:\n{rendered}"
        );
    }
    for label in ["parent", "depends", "next"] {
        assert!(
            rendered.contains(label),
            "horizontal routing must preserve {label:?}:\n{rendered}"
        );
    }
}