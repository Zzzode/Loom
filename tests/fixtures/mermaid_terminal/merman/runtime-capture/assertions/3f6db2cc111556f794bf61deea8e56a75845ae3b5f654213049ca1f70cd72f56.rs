fn er_fallback_rolls_back_speculative_document_cells() {
    let source = "erDiagram\nA\nB\nA ||--o{ B : owns";
    let exact_resources = merman_ascii::AsciiResourcePolicy::unbounded()
        .with_limit(merman_ascii::AsciiResourceLimitId::MaxDocumentCells, 1_188)
        .expect("limit should be valid");
    let report = render_report_with_resources(
        source,
        AsciiViewportPolicy::with_max_width(4).overflow(OverflowPolicy::Fallback),
        exact_resources,
    )
    .expect("ER fallback should not count the abandoned primary document twice");
    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);

    let below_resources = merman_ascii::AsciiResourcePolicy::unbounded()
        .with_limit(merman_ascii::AsciiResourceLimitId::MaxDocumentCells, 1_187)
        .expect("limit should be valid");
    let error = render_report_with_resources(
        source,
        AsciiViewportPolicy::with_max_width(4).overflow(OverflowPolicy::Fallback),
        below_resources,
    )
    .expect_err("one fewer ER fallback document cell should fail");
    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(details)
            if details.limit == merman_ascii::AsciiResourceLimitId::MaxDocumentCells
                && details.actual == 1_188
                && details.max == 1_187
    ));
}