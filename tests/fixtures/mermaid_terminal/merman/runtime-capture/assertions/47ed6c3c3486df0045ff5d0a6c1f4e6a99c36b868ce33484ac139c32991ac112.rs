fn sequence_fallback_preserves_primary_work_in_the_render_wide_ledger() {
    let source = concat!(
        "sequenceDiagram\n",
        "participant Alice\n",
        "participant Bob\n",
        "Alice->>Bob: Request with a long authored message\n",
        "Bob-->>Alice: Response with another long authored message\n",
    );
    let exact_work = 4_082;
    let exact_resources = merman_ascii::AsciiResourcePolicy::unbounded()
        .with_limit(
            merman_ascii::AsciiResourceLimitId::MaxLayoutWorkUnits,
            exact_work,
        )
        .expect("limit should be valid");

    let report = render_report_with_resources(
        source,
        AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
        exact_resources,
    )
    .expect("the exact cumulative primary-plus-fallback work boundary should succeed");
    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);

    let below_resources = merman_ascii::AsciiResourcePolicy::unbounded()
        .with_limit(
            merman_ascii::AsciiResourceLimitId::MaxLayoutWorkUnits,
            exact_work - 1,
        )
        .expect("limit should be valid");
    let error = render_report_with_resources(
        source,
        AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
        below_resources,
    )
    .expect_err("the N-1 cumulative work boundary should fail");

    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(details)
            if details.limit == merman_ascii::AsciiResourceLimitId::MaxLayoutWorkUnits
                && details.actual == exact_work
                && details.max == exact_work - 1
    ));
}