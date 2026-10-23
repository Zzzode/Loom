fn flowchart_local_semantic_fixture_covers_nested_direction_boundary_routes() {
    let input = local_semantic_input("flowchart/nested_direction_boundary.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic nested flowchart fixture should render");

    for expected in [
        "Start",
        "Outer Pipeline",
        "Inner Steps",
        "Entry",
        "Validate",
        "Persist",
        "Done",
    ] {
        assert!(
            rendered.contains(expected),
            "nested flowchart fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    let position = |needle: &str| {
        rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(needle).map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing {needle:?} in output:\n{rendered}"))
    };
    let start = position("Start");
    let entry = position("Entry");
    let validate = position("Validate");
    let persist = position("Persist");
    let done = position("Done");

    assert!(
        start.1 < entry.1,
        "root TD direction should keep Start above the outer group entry:\n{rendered}"
    );
    assert_eq!(
        entry.1, validate.1,
        "the explicit outer LR direction should survive boundary edges:\n{rendered}"
    );
    assert!(
        validate.1 < persist.1,
        "inner TD override should keep Validate above Persist:\n{rendered}"
    );
    assert!(
        persist.1 < done.1,
        "cross-boundary exit edge should keep Done after Persist in root TD flow:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 10,
        "local semantic flowchart fixture should produce a non-trivial layout:\n{rendered}"
    );
}