fn bidirectional_state_labels_keep_reading_clearance() {
    let rendered = render_state(
        "stateDiagram-v2\n Draft --> Review: submit\n Review --> Draft: changes requested\n",
        &AsciiRenderOptions::unicode(),
    )
    .unwrap();
    assert_eq!(rendered.matches("submit").count(), 1);
    assert_eq!(rendered.matches("changes requested").count(), 1);
    assert!(!rendered.contains("submitchanges requested"), "{rendered}");
    assert!(!rendered.contains("changes requestedsubmit"), "{rendered}");
}