fn class_local_semantic_fixture_covers_disconnected_components() {
    let input = read_local_semantic_fixture("class/disconnected_components.mmd");

    let rendered = render_class(&input, &AsciiRenderOptions::ascii())
        .expect("disconnected class fixture should render");

    for expected in ["Service", "Repo", "Logger", "Isolated", "fetch", "log"] {
        assert!(
            rendered.contains(expected),
            "disconnected class fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("relations:"),
        "disconnected class fixture should stay as a routed grid, not a summary:\n{rendered}"
    );

    let line_index = |needle: &str| {
        rendered
            .lines()
            .position(|line| line.contains(needle))
            .unwrap_or_else(|| panic!("missing {needle:?} in rendered fixture:\n{rendered}"))
    };
    assert!(
        line_index("Service") < line_index("Isolated"),
        "isolated class component should remain visually separate from the connected component:\n{rendered}"
    );
}