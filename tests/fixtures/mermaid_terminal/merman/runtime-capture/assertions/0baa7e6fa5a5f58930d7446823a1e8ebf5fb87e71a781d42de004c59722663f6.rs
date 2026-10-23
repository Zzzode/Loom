fn sequence_autonumber_accepts_decimal_start_and_step() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nautonumber 10.1 .01\nA->>B: First\nB-->>A: Second\nA->>B: Third",
        &AsciiRenderOptions::unicode(),
    )
    .expect("sequence decimal autonumber should render");
    let normalized = normalize_sequence_output(&rendered);

    assert!(
        normalized.contains("10.1. First"),
        "expected decimal start in ASCII output:\n{normalized}"
    );
    assert!(
        normalized.contains("10.11. Second"),
        "expected rounded decimal step in ASCII output:\n{normalized}"
    );
    assert!(
        normalized.contains("10.12. Third"),
        "expected second rounded decimal step in ASCII output:\n{normalized}"
    );
    assert!(
        !normalized.contains("10.110000"),
        "expected decimal labels to avoid floating point artifacts:\n{normalized}"
    );
}