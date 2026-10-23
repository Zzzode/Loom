fn state_composite_explicit_reverse_direction_mirrors_child_layout() {
    let rendered = render_state(
        concat!(
            "stateDiagram-v2\n",
            "direction TB\n",
            "state Outer {\n",
            "  direction RL\n",
            "  A --> B\n",
            "}\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("explicit reverse state direction should render");

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
        "explicit RL direction should keep child states on one row:\n{rendered}"
    );
    assert!(
        b.0 < a.0,
        "explicit RL direction should place B before A:\n{rendered}"
    );
}