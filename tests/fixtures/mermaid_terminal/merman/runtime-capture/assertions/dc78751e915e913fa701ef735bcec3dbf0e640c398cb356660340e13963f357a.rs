fn sequence_deactivate_after_destroy_preserves_parser_activation_semantics() {
    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "participant A\n",
            "participant B\n",
            "activate B\n",
            "A->>B: Work\n",
            "destroy B\n",
            "A--xB: Bye\n",
            "deactivate B",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .expect("deactivation after a destroy signal should match Mermaid parser semantics");

    assert!(rendered.contains("Work"), "{rendered}");
    assert!(
        rendered.contains("Bye") && rendered.contains('×'),
        "{rendered}"
    );
}