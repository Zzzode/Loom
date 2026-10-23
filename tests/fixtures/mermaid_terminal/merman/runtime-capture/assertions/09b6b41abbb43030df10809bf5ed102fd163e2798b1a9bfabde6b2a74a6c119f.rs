fn graph_fixture_exact_subset_matches_upstream() {
    let gaps = graph_fixture_keys(GRAPH_FIXTURE_GAPS);
    let corrections = graph_fixture_keys(GRAPH_FIXTURE_CORRECTIONS);
    let mut exact_count = 0;
    let mut mismatches = Vec::new();
    for fixture in GRAPH_FIXTURE_CORPUS {
        if gaps.contains(&fixture.key()) || corrections.contains(&fixture.key()) {
            continue;
        }
        exact_count += 1;
        let path = fixture_path(*fixture);
        let (input, expected) = split_fixture(&path);
        match render_flowchart(&input, &fixture.options()) {
            Ok(rendered) if rendered == expected => {}
            Ok(_) => mismatches.push(format!("{}: output differs", fixture.key())),
            Err(error) => mismatches.push(format!("{}: {error}", fixture.key())),
        }
    }

    assert_eq!(
        exact_count, 33,
        "the immutable exact subset must remain explicit"
    );
    assert!(
        mismatches.is_empty(),
        "copied graph fixtures no longer match exactly:\n{}",
        mismatches.join("\n")
    );
}