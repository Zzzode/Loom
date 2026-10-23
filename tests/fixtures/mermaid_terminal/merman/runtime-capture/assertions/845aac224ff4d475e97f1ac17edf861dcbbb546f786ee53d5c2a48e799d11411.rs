fn flowchart_parser_explicit_subgraph_direction_survives_cross_boundary_edges() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TD\n",
            "subgraph one [LR Group]\n",
            "    direction LR\n",
            "    A --> B\n",
            "end\n",
            "X --> A\n",
            "B --> Y\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("cross-boundary subgraph should retain its explicit local direction");

    for expected in ["LR", "Group", "A", "B", "X", "Y"] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
    let position = |needle: &str| {
        rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(needle).map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing {needle:?} in output:\n{rendered}"))
    };
    let x = position(" X ");
    let a = position(" A ");
    let b = position(" B ");
    let y = position(" Y ");
    assert!(
        x.1 < a.1,
        "root TD direction should keep the external source above its group entry:\n{rendered}"
    );
    assert_eq!(
        a.1, b.1,
        "an explicit LR subgraph must remain horizontal across boundary edges:\n{rendered}"
    );
    assert!(
        b.1 < y.1,
        "root TD direction should keep the external target below its group exit:\n{rendered}"
    );
    assert_rectangular_char_grid(&rendered);
}