fn auto_sequence_retains_canonical_document_under_the_same_exact_cell_budget() {
    use merman_ascii::{AsciiRenderer, AsciiResourceLimitId, AsciiResourcePolicy};
    use merman_core::{OperationControl, runtime::RuntimePolicy};
    use unicode_width::UnicodeWidthStr;

    let model = parse_model(&local_semantic_input(
        "sequence/self_messages_with_notes.mmd",
    ));
    let options = AsciiRenderOptions::ascii();
    let canonical = render_model(&model, &options).expect("canonical sequence should render");
    let compact = render_model(
        &model,
        &options.with_layout_profile(AsciiLayoutProfile::Compact),
    )
    .expect("compact sequence should render");
    let document_cells = |text: &str| text.lines().map(UnicodeWidthStr::width).sum::<usize>();
    let canonical_cells = document_cells(&canonical);
    let compact_cells = document_cells(&compact);
    assert!(canonical_cells > compact_cells);

    // Auto with Allow retains the complete Canonical document while admitting Compact.
    // The fixed-profile case distinguishes candidate accounting from retained storage.
    for (profile, exact_cells) in [
        (AsciiLayoutProfile::Compact, compact_cells),
        (AsciiLayoutProfile::Auto, canonical_cells + compact_cells),
    ] {
        let renderer =
            AsciiRenderer::new(options.with_layout_profile(profile)).expect("valid layout profile");
        let render_with_budget = |limit| {
            let context = RuntimePolicy::deterministic()
                .begin_operation()
                .expect("deterministic test operation context");
            renderer.render_model_report(
                &model,
                AsciiViewportPolicy::with_max_width(105).overflow(OverflowPolicy::Allow),
                &OperationControl::new(),
                &context,
                AsciiResourcePolicy::default()
                    .with_limit(AsciiResourceLimitId::MaxDocumentCells, limit)
                    .expect("positive document cell budget"),
            )
        };
        let output = render_with_budget(exact_cells).unwrap_or_else(|error| {
            panic!("{profile:?} must fit exactly {exact_cells} cells: {error}")
        });
        assert_eq!(output.text, compact);
        assert_eq!(output.layout_profile, AsciiLayoutProfile::Compact);
        assert_eq!(output.requested_layout_profile, profile);
        assert_eq!(
            output.compact_attempted,
            profile == AsciiLayoutProfile::Auto
        );
        assert_eq!(
            (output.primary_extent.width, output.primary_extent.height),
            (105, 58)
        );
        assert_eq!(output.emitted_extent, output.primary_extent);
        assert!(!output.overflowed);
        assert!(!output.fallback.attempted);

        let error = render_with_budget(exact_cells - 1)
            .expect_err("one cell below retained plus selected documents must exhaust the budget");
        assert!(
            matches!(
                error,
                AsciiError::ResourceLimitExceeded(ref exceeded)
                    if exceeded.limit == AsciiResourceLimitId::MaxDocumentCells
                        && exceeded.max == exact_cells - 1
                        && exceeded.actual > exceeded.max
            ),
            "{profile:?} returned an unexpected error with {} cells: {error:?}",
            exact_cells - 1
        );
    }
}