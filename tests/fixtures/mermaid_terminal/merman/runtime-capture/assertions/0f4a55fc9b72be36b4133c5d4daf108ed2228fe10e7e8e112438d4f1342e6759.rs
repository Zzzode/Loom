fn sequence_extended_signal_markers_render_from_typed_endpoint_semantics() {
    let rendered = render_sequence(
        r#"sequenceDiagram
participant A
participant B
A-)B: Async point
A<<->>B: Bidirectional
A-|\B: Filled half
A/|-B: Reverse half
A--//B: Open half
A--)A: Self async"#,
        &AsciiRenderOptions::unicode(),
    )
    .expect("extended sequence signal markers should render");

    for label in [
        "Async point",
        "Bidirectional",
        "Filled half",
        "Reverse half",
        "Open half",
        "Self async",
    ] {
        assert!(
            rendered.contains(label),
            "extended signal label {label:?} should remain visible:\n{rendered}"
        );
    }
    assert!(
        rendered.contains(')'),
        "async point markers should remain distinct:\n{rendered}"
    );
    assert!(
        rendered.contains("├◄") && rendered.contains("►│"),
        "bidirectional messages should paint both endpoint markers:\n{rendered}"
    );
    assert!(
        rendered.contains('◢') && rendered.contains('◣'),
        "forward and reverse filled half markers should retain endpoint ownership:\n{rendered}"
    );
    assert!(
        rendered.contains('╱'),
        "open half markers should remain distinct from filled halves:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.contains('┈') && line.contains('(')),
        "dotted self messages should preserve their point marker and stroke:\n{rendered}"
    );
}