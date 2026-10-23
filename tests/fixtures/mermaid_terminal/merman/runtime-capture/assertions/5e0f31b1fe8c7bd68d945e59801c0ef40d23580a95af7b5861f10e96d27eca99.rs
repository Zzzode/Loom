fn class_local_semantic_fixture_covers_dense_multiline_relation_summary() {
    let input = read_local_semantic_fixture("class/dense_multiline_relations.mmd");

    let rendered = render_class(&input, &AsciiRenderOptions::ascii())
        .expect("dense multiline local semantic class fixture should render");

    for expected in [
        "Gateway",
        "Service",
        "Repo",
        "Cache",
        "relations:",
        r#"id(bytes=7)="Gateway" --> id(bytes=7)="Service" : receives"#,
        "request",
        r#"id(bytes=7)="Service" --> id(bytes=7)="Gateway" : returns"#,
        "response",
        "persists",
        "state",
        "invalidates",
        "entry",
    ] {
        assert!(
            rendered.contains(expected),
            "dense multiline semantic class fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains(" / "),
        "dense multiline semantic class fixture should keep label lines structured instead of slash-joining them:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .all(|line| !line.contains("<br>") || line.contains("authored(bytes=")),
        "dense multiline semantic class fixture should expose Mermaid break syntax only inside authored framing:\n{rendered}"
    );
}