fn er_parser_horizontal_component_draws_each_entity_once() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "direction LR\n",
            "A ||--o{ B : owns\n",
            "A |o..|{ B : \"may own\"\n",
            "B ||--|| C : joins",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal ER component should render");

    for title in ["| A |", "| B |", "| C |"] {
        assert_eq!(
            rendered.matches(title).count(),
            1,
            "a horizontal ER component must place {title:?} exactly once:\n{rendered}"
        );
    }
    for label in ["owns", "may own", "joins"] {
        assert!(
            rendered.contains(label),
            "horizontal ER routing must preserve {label:?}:\n{rendered}"
        );
    }
}