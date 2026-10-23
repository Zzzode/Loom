fn sequence_distinct_id_and_actor_character_sets_render_without_loss() {
    let input = r#"sequenceDiagram
participant C++
participant api(v2)
participant api-xray
activate api-xray
alice@example.com->>data@example.com: mail
deactivate api-xray"#;

    let rendered = render_sequence(input, &AsciiRenderOptions::ascii())
        .expect("pinned ID and ACTOR character sets should survive terminal projection");
    for expected in [
        "C++",
        "api(v2)",
        "api-xray",
        "alice@example.com",
        "data@example.com",
        "mail",
    ] {
        assert!(
            rendered.contains(expected),
            "sequence output should retain {expected:?}:\n{rendered}"
        );
    }
}