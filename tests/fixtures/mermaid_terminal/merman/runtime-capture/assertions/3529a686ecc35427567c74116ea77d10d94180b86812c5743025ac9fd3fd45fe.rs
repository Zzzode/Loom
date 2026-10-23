fn er_local_semantic_fixture_covers_disconnected_components() {
    let input = read_local_semantic_fixture("er/disconnected_components.mmd");

    let rendered = render_er(&input, &AsciiRenderOptions::ascii())
        .expect("disconnected ER fixture should render");

    for expected in ["CUSTOMER", "ORDER", "AUDIT_LOG", "places"] {
        assert!(
            rendered.contains(expected),
            "disconnected ER fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "disconnected ER fixture should stay as a routed grid, not a summary:\n{rendered}"
    );

    let line_index = |needle: &str| {
        rendered
            .lines()
            .position(|line| line.contains(needle))
            .unwrap_or_else(|| panic!("missing {needle:?} in rendered fixture:\n{rendered}"))
    };
    assert!(
        line_index("CUSTOMER") < line_index("AUDIT_LOG"),
        "isolated ER entity should remain visually separate from the connected component:\n{rendered}"
    );
}