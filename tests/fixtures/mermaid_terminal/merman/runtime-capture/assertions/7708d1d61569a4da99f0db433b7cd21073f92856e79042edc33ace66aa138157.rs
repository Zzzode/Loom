fn sequence_ascii_self_half_arrow_expands_a_narrow_loop_without_losing_fill() {
    let mut options = AsciiRenderOptions::ascii();
    options.sequence_self_message_width = 2;
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nA-|\\A: Self filled half",
        &options,
    )
    .expect("filled self half-arrow should expand its effective loop width");

    assert!(rendered.contains("Self filled half"));
    assert!(
        rendered.contains("/|"),
        "self half-arrow should retain its filled stem:\n{rendered}"
    );
}