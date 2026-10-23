fn state_nested_composite_inherits_nearest_non_root_explicit_direction() {
    let rendered = render_state(
        concat!(
            "stateDiagram-v2\n",
            "direction TB\n",
            "state Outer {\n",
            "  direction LR\n",
            "  state Inner {\n",
            "    A --> B\n",
            "  }\n",
            "}\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("implicit nested composite should inherit the nearest explicit ancestor direction");

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
        a.1, b.1,
        "Inner must inherit Outer LR instead of falling back to root TB:\n{rendered}"
    );
}