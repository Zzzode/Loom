fn sequence_sectioned_control_blocks_frame_multiple_sections_and_notes() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nalt Primary path\nA->>B: First\nelse Secondary path\nNote over A,B: Wait\nelse Tertiary path\nB-->>A: Third\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("alt should render multiple sections and notes");

    for marker in ["├ else Secondary path ", "├ else Tertiary path "] {
        assert!(
            rendered.lines().any(|line| line.starts_with(marker)),
            "alt should render every section separator:\n{rendered}"
        );
    }
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains("Wait")),
        "alt should keep note rows inside sectioned frames:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains("Third")),
        "alt should keep later section messages inside the frame:\n{rendered}"
    );
}