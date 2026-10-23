fn auto_sequence_with_notes_and_self_messages_respects_required_label_space() {
    let model = parse_model(&local_semantic_input(
        "sequence/self_messages_with_notes.mmd",
    ));
    let options = AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Auto);
    let output = render_model_report(
        &model,
        &options,
        AsciiViewportPolicy::with_max_width(105).overflow(OverflowPolicy::Error),
    )
    .expect("Compact should fit the note/self-message diagram");
    assert_eq!(output.layout_profile, AsciiLayoutProfile::Compact);
    assert!(output.compact_attempted);
    assert_eq!(
        (output.emitted_extent.width, output.emitted_extent.height),
        (105, 58)
    );
    for label in [
        "event.preventDefault()",
        "WINDOW_CLOSE_REQUESTED",
        "Panel removed",
        "Panel reopens",
    ] {
        assert!(output.text.contains(label), "lost {label}: {}", output.text);
    }
    assert!(!output.fallback.attempted);
}