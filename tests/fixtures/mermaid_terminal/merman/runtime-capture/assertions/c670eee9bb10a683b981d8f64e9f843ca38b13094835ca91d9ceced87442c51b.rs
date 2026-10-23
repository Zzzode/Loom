fn state_nested_explicit_direction_overrides_its_explicit_ancestor() {
    let rendered = render_state(
        concat!(
            "stateDiagram-v2\n",
            "direction TB\n",
            "state Outer {\n",
            "  direction LR\n",
            "  state Inner {\n",
            "    direction TB\n",
            "    A --> B\n",
            "  }\n",
            "}\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("explicit nested composite direction should override its ancestor");

    let a = rendered
        .lines()
        .enumerate()
        .find_map(|(y, line)| line.find(" A ").map(|x| (x, y)))
        .expect("missing state A");
    let b = rendered
        .lines()
        .enumerate()
        .find_map(|(y, line)| line.find(" B ").map(|x| (x, y)))
        .expect("missing state B");
    assert_eq!(
        a.0, b.0,
        "Inner TB must override Outer LR instead of being flattened by it:\n{rendered}"
    );
    assert!(
        a.1 < b.1,
        "Inner TB must keep authored source-before-target order:\n{rendered}"
    );
}