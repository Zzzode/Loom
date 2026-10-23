fn class_local_semantic_fixture_covers_note_for_link() {
    let input = read_local_semantic_fixture("class/note_for_service.mmd");
    let rendered =
        render_class(&input, &AsciiRenderOptions::ascii()).expect("class diagram should render");

    for expected in [
        "note",
        "Handles",
        "requests",
        "Service",
        r#"authored(bytes=19)="Handles<br>requests""#,
    ] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
    assert!(rendered.contains(':'), "{rendered}");
}