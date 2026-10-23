fn sequence_wrapped_messages_respect_display_width_for_cjk() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>B:wrap: 数据数据数据数据",
        &AsciiRenderOptions::unicode(),
    )
    .expect("wrapped CJK sequence messages should render");

    assert!(
        !rendered.contains("数据数据数据数据"),
        "wide text without spaces should wrap by display width:\n{rendered}"
    );
}