#[test]
fn tier_a_corpus_produces_deterministic_reports() {
    let reports = report::run_corpus(mutations::tier_a_pairs()).expect("corpus should run");
    let class_reports =
        report::run_corpus(mutations::class_canaries()).expect("class canaries should run");

    assert!(
        reports.len() >= 19,
        "Tier A should include M01-M12, M14-M15, M05-ID, and S01-S04"
    );
    assert_eq!(
        class_reports.len(),
        2,
        "M16/M17 class canaries are disjoint from Tier A"
    );
    assert!(reports.iter().any(|report| report.pair_id == "M10"));
    assert!(class_reports.iter().any(|report| report.pair_id == "M16"));
    assert!(class_reports.iter().any(|report| report.pair_id == "M17"));
    assert!(reports.iter().all(|report| !report.summary().is_empty()));
    assert!(
        class_reports
            .iter()
            .all(|report| !report.summary().is_empty())
    );

    let first = report::format_reports(&reports);
    let second = report::format_reports(&reports);
    assert_eq!(first, second);
}