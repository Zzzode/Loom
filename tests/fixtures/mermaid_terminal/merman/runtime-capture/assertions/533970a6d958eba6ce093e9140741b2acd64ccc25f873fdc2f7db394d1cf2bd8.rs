fn xychart_imported_fixture_matrix_remains_smoke_green() {
    let fixture_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../fixtures/xychart");
    let mut fixtures = std::fs::read_dir(&fixture_dir)
        .unwrap_or_else(|error| panic!("failed to read {}: {error}", fixture_dir.display()))
        .map(|entry| {
            entry
                .expect("fixture directory entry should be readable")
                .path()
        })
        .filter(|path| path.extension().is_some_and(|extension| extension == "mmd"))
        .collect::<Vec<_>>();
    fixtures.sort();
    assert_eq!(
        fixtures.len(),
        74,
        "the pinned XYChart fixture inventory changed; update the semantic gate intentionally"
    );

    let mut failures = Vec::new();
    for fixture in fixtures {
        let input = std::fs::read_to_string(&fixture)
            .unwrap_or_else(|error| panic!("failed to read {}: {error}", fixture.display()));
        if let Err(error) = render_xychart(&input, &AsciiRenderOptions::ascii()) {
            failures.push(format!(
                "{}: {error}",
                fixture
                    .file_name()
                    .and_then(|name| name.to_str())
                    .unwrap_or("<non-utf8 fixture>")
            ));
        }
    }

    assert!(
        failures.is_empty(),
        "imported XYChart fixtures failed ASCII smoke rendering:\n{}",
        failures.join("\n")
    );
}