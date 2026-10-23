fn state_composite_without_explicit_direction_inherits_nearest_explicit_ancestor() {
    let rendered = render_state(
        concat!(
            "stateDiagram-v2\n",
            "direction LR\n",
            "state Outer {\n",
            "  state Inner {\n",
            "    A --> B\n",
            "  }\n",
            "}\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("nested state composites should inherit the root direction");

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
        "nearest explicit LR direction should apply to the nested composite:\n{rendered}"
    );
}