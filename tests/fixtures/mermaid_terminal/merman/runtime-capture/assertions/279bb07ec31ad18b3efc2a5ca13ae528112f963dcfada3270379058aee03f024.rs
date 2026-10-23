fn sequence_default_keeps_mermaid_mirror_actors_disabled() {
    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "    participant U as User\n",
            "    participant B as Browser\n",
            "    participant S as Server\n",
            "    U->>B: Click Login\n",
            "    B->>S: POST /login\n",
            "    S-->>B: Return Token\n",
            "    B-->>U: Show Success\n",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .unwrap();

    assert!(
        !rendered.contains("┌───┴──┐"),
        "default Mermaid-compatible sequence output should not mirror actors: {rendered}"
    );
}