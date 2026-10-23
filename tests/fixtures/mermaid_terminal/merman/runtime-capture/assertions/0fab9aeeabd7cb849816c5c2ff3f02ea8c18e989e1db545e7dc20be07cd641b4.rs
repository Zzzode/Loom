fn sequence_compact_candidate_reduces_width_area_and_blank_cells() {
    let source = local_semantic_input("sequence/self_messages_with_notes.mmd");
    let model = parse_model(&source);
    let canonical = render_model(&model, &AsciiRenderOptions::ascii())
        .expect("canonical Sequence evidence fixture should render");
    let compact = render_model(
        &model,
        &AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Compact),
    )
    .expect("compact Sequence evidence fixture should render");

    let canonical_extent = terminal_extent(&canonical);
    let compact_extent = terminal_extent(&compact);
    let (canonical_blank_cells, canonical_longest_blank_run) = blank_cell_metrics(&canonical);
    let (compact_blank_cells, compact_longest_blank_run) = blank_cell_metrics(&compact);

    assert_eq!(canonical_extent, (107, 58));
    assert_eq!(compact_extent, (105, 58));
    assert_eq!(canonical_blank_cells, 4_681);
    assert_eq!(compact_blank_cells, 4_567);
    assert_eq!(canonical_longest_blank_run, 41);
    assert_eq!(compact_longest_blank_run, 41);
    assert_sequence_message_lifelines(&model, &canonical, TerminalWidthProfile::Unicode);
    assert_sequence_message_lifelines(&model, &compact, TerminalWidthProfile::Unicode);
    assert!(compact_extent.0 < canonical_extent.0);
    assert_eq!(compact_extent.1, canonical_extent.1);
    assert!(compact_extent.0 * compact_extent.1 < canonical_extent.0 * canonical_extent.1);
    assert!(compact_blank_cells < canonical_blank_cells);
}