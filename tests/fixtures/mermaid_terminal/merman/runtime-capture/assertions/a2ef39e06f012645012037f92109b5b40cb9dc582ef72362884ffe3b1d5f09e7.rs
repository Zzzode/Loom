fn sequence_autonumber_off_preserves_and_advances_hidden_state() {
    let rendered = render_sequence(
        r#"sequenceDiagram
participant A
participant B
autonumber 10 5
A->>B: Visible first
autonumber off
A->>B: Hidden first
B-->>A: Hidden second
autonumber
A->>B: Visible resumed"#,
        &AsciiRenderOptions::unicode(),
    )
    .expect("sequence autonumber visibility changes should render");
    let normalized = normalize_sequence_output(&rendered);

    assert!(
        normalized.contains("10. Visible first"),
        "expected configured autonumber start before disabling it:\n{normalized}"
    );
    assert!(
        normalized.contains("Hidden first")
            && !normalized.contains("15. Hidden first")
            && normalized.contains("Hidden second")
            && !normalized.contains("20. Hidden second"),
        "hidden signals should advance the counter without displaying it:\n{normalized}"
    );
    assert!(
        normalized.contains("25. Visible resumed"),
        "reenabling autonumber should retain the counter and step across hidden signals:\n{normalized}"
    );
}