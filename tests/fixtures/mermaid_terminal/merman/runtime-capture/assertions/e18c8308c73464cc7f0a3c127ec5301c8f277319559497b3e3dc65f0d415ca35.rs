fn flowchart_local_semantic_fixture_covers_multiple_boundary_routes() {
    let input = local_semantic_input("flowchart/multi_boundary_routes.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic multi-boundary flowchart fixture should render");

    for expected in [
        "Source", "Audit", "Pipeline", "Ingest", "Validate", "Publish", "Success", "Retry", "load",
        "check", "ok", "fail",
    ] {
        assert!(
            rendered.contains(expected),
            "multi-boundary flowchart fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    let position = |needle: &str| {
        rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(needle).map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing {needle:?} in output:\n{rendered}"))
    };
    let source = position("Source");
    let audit = position("Audit");
    let ingest = position("Ingest");
    let validate = position("Validate");
    let publish = position("Publish");
    let success = position("Success");
    let retry = position("Retry");

    assert!(
        source.1 < ingest.1,
        "first entering boundary edge should preserve root TD ordering:\n{rendered}"
    );
    assert!(
        audit.1 < validate.1,
        "second entering boundary edge should preserve root TD ordering:\n{rendered}"
    );
    assert_eq!(
        ingest.1, validate.1,
        "the explicit Pipeline LR direction should survive entering edges:\n{rendered}"
    );
    assert_eq!(
        validate.1, publish.1,
        "the explicit Pipeline LR direction should survive leaving edges:\n{rendered}"
    );
    assert!(
        publish.1 < success.1,
        "first leaving boundary edge should preserve root TD ordering:\n{rendered}"
    );
    assert!(
        publish.1 < retry.1,
        "second leaving boundary edge should preserve root TD ordering:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 10,
        "multi-boundary flowchart fixture should produce a non-trivial layout:\n{rendered}"
    );
}