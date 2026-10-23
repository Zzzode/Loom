fn graph_fixture_named_corrections_preserve_closed_compound_frames() {
    for fixture in GRAPH_FIXTURE_CORRECTIONS {
        let (input, copied) = split_fixture(&fixture_path(*fixture));
        let snapshot = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("tests/graph_fixture/corrected-fixtures")
            .join(fixture.directory)
            .join(fixture.name);
        let expected = std::fs::read_to_string(&snapshot).unwrap_or_else(|error| {
            panic!("read corrected snapshot {}: {error}", snapshot.display())
        });
        assert_ne!(
            expected,
            copied,
            "{} must retain a verified correction",
            fixture.key()
        );
        assert_eq!(
            expected.len(),
            copied.len(),
            "these corrections preserve the copied geometry"
        );
        assert!(
            copied
                .chars()
                .zip(expected.chars())
                .all(|(original, corrected)| {
                    original == corrected || (matches!(original, '-' | '|') && corrected == '+')
                }),
            "{} must only preserve the frame directions at route crossings",
            fixture.key()
        );
        let rendered = render_flowchart(&input, &fixture.options())
            .unwrap_or_else(|error| panic!("{}: {error}", fixture.key()));
        assert_eq!(
            rendered,
            expected,
            "{} corrected output changed",
            fixture.key()
        );
        assert_eq!(
            visible_text_token_counts(&rendered),
            visible_text_token_counts(&copied),
            "{} must preserve every copied label exactly once",
            fixture.key()
        );
        assert_corrected_compound_geometry(&input, &rendered);
    }
}