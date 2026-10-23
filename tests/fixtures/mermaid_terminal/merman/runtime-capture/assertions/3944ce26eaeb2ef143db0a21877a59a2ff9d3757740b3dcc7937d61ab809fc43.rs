fn issue_53_compact_candidate_reduces_width_and_total_blank_cells() {
    let source = local_semantic_input("flowchart/issue_53_long_node_labels.mmd");
    let model = parse_model(&source);
    let canonical = render_model(&model, &AsciiRenderOptions::ascii())
        .expect("canonical Issue #53 fixture should render");
    let compact = render_model(
        &model,
        &AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Compact),
    )
    .expect("compact Issue #53 fixture should preserve route clearance");

    let canonical_extent = terminal_extent(&canonical);
    let compact_extent = terminal_extent(&compact);
    let (canonical_blank_cells, canonical_longest_blank_run) = blank_cell_metrics(&canonical);
    let (compact_blank_cells, compact_longest_blank_run) = blank_cell_metrics(&compact);

    assert_eq!(canonical_extent, (74, 57));
    assert_eq!(compact_extent, (56, 67));
    assert_eq!(canonical_blank_cells, 3_236);
    assert_eq!(compact_blank_cells, 2_910);
    assert_eq!(canonical_longest_blank_run, 51);
    assert_eq!(compact_longest_blank_run, 41);
    assert!(compact_extent.0 < canonical_extent.0);
    assert!(compact_extent.1 > canonical_extent.1);
    assert!(compact_extent.0 * compact_extent.1 < canonical_extent.0 * canonical_extent.1);
    assert!(compact_blank_cells < canonical_blank_cells);
    assert!(compact_longest_blank_run < canonical_longest_blank_run);
}