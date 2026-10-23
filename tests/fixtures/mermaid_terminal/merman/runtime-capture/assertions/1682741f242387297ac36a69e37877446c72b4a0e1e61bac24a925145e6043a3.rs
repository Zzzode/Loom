fn reverse_edge_labels_keep_reading_clearance() {
    let rendered = render_flowchart(
        "flowchart TD\n Draft -->|submit| Review\n Review -->|changes requested| Draft\n",
        &AsciiRenderOptions::unicode(),
    )
    .unwrap();
    assert_eq!(rendered.matches("submit").count(), 1);
    assert_eq!(rendered.matches("changes requested").count(), 1);
    assert!(!rendered.contains("submitchanges requested"), "{rendered}");
    assert!(!rendered.contains("changes requestedsubmit"), "{rendered}");
}