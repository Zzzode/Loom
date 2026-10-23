fn flowchart_local_semantic_fixture_covers_boundary_label_lane() {
    let input = local_semantic_input("flowchart/boundary_label_lane.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic boundary-label lane fixture should render");

    for expected in [
        "Start",
        "Pipeline",
        "Ingest",
        "Publish",
        "Success",
        "boundaryLabelWithEnoughWidth",
    ] {
        assert!(
            rendered.contains(expected),
            "boundary-label lane fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    let label_line = rendered
        .lines()
        .find(|line| line.contains("boundaryLabelWithEnoughWidth"))
        .unwrap_or_else(|| panic!("boundary label should render:\n{rendered}"));
    let rows: Vec<_> = rendered.lines().collect();
    let label_y = rows.iter().position(|line| *line == label_line).unwrap();
    let label_x = label_line.find("boundaryLabelWithEnoughWidth").unwrap();
    let host_continues = (label_x..label_x + "boundaryLabelWithEnoughWidth".len()).any(|x| {
        rows[label_y - 1].as_bytes().get(x) == Some(&b'|')
            && rows[label_y + 1].as_bytes().get(x) == Some(&b'|')
    });
    assert!(
        label_line.contains("|boundaryLabelWithEnoughWidth") || host_continues,
        "boundary label should stay on its vertical transit host:\n{rendered}"
    );

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert!(
        line_index("Publish") < line_index("boundaryLabelWithEnoughWidth"),
        "boundary label should render after the source endpoint on the boundary lane:\n{rendered}"
    );
    assert!(
        line_index("boundaryLabelWithEnoughWidth") < line_index("Success"),
        "boundary label should render before the target endpoint on the boundary lane:\n{rendered}"
    );
}