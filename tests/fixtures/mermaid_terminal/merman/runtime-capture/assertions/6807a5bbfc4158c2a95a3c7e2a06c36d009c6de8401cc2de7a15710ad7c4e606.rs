fn er_local_semantic_fixture_covers_dense_multiline_relation_summary() {
    let input = read_local_semantic_fixture("er/dense_multiline_relations.mmd");

    let rendered = render_er(&input, &AsciiRenderOptions::ascii())
        .expect("dense multiline local semantic ER fixture should render");

    for expected in [
        "CUSTOMER".to_string(),
        "ORDER".to_string(),
        "INVOICE".to_string(),
        "PAYMENT".to_string(),
        "relations:".to_string(),
        format!(
            "{} ||--o{{ {}",
            framed_er_summary_endpoint("CUSTOMER"),
            framed_er_summary_endpoint("ORDER")
        ),
        "places".to_string(),
        "orders".to_string(),
        "belongs".to_string(),
        "to".to_string(),
        "reconciles".to_string(),
        "payment".to_string(),
        "captures".to_string(),
        "funds".to_string(),
    ] {
        assert!(
            rendered.contains(&expected),
            "dense multiline semantic ER fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains(" / "),
        "dense multiline semantic ER fixture should keep label lines structured instead of slash-joining them:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .all(|line| !line.contains("<br>") || line.contains("authored(bytes=")),
        "dense multiline semantic ER fixture should expose Mermaid break syntax only inside authored framing:\n{rendered}"
    );
}