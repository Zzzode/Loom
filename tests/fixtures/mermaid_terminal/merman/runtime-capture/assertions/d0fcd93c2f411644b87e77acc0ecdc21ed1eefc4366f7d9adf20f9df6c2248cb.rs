fn flowchart_parser_sibling_groups_keep_explicit_directions_across_external_edges() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TD\n",
            "subgraph left [Left Group]\n",
            "    direction LR\n",
            "    A[Alpha] --> B[Beta]\n",
            "end\n",
            "subgraph right [Right Group]\n",
            "    direction RL\n",
            "    C[Gamma] --> D[Delta]\n",
            "end\n",
            "B -- handoff --> C\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("externally connected sibling groups should retain explicit local directions");

    for expected in [
        "Left Group",
        "Right Group",
        "Alpha",
        "Beta",
        "Gamma",
        "Delta",
        "handoff",
    ] {
        assert!(
            rendered.contains(expected),
            "mixed-direction sibling group output should keep {expected:?} visible:\n{rendered}"
        );
    }

    let position = |needle: &str| {
        rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(needle).map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing {needle:?} in output:\n{rendered}"))
    };
    let alpha = position("Alpha");
    let beta = position("Beta");
    let gamma = position("Gamma");
    let delta = position("Delta");

    assert_eq!(
        alpha.1, beta.1,
        "the explicit left sibling LR direction should survive external edges:\n{rendered}"
    );
    assert!(
        alpha.0 < beta.0,
        "the left sibling should preserve authored LR order:\n{rendered}"
    );
    assert_eq!(
        gamma.1, delta.1,
        "the explicit right sibling RL direction should survive external edges:\n{rendered}"
    );
    assert!(
        gamma.0 > delta.0,
        "the right sibling should preserve authored RL order:\n{rendered}"
    );
}