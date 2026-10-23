fn sequence_central_marker_records_are_suppressed_without_skipping_autonumbers() {
    let rendered = render_sequence(
        r#"sequenceDiagram
participant A
participant B
autonumber
A->>()B: Target central
A()->>B: Source central
B()->>()A: Dual central"#,
        &AsciiRenderOptions::unicode(),
    )
    .expect("central connection decorations should render");

    for numbered_label in ["1. Target central", "2. Source central", "3. Dual central"] {
        assert!(
            rendered.contains(numbered_label),
            "central marker records must not consume autonumbers for {numbered_label:?}:\n{rendered}"
        );
    }
    assert!(
        rendered.matches('○').count() >= 4,
        "target, source, and dual central decorations should remain visible:\n{rendered}"
    );
}