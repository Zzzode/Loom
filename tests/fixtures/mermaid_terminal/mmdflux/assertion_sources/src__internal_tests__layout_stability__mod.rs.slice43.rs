#[test]
fn report_summary_distinguishes_compared_paths_from_changed_paths() {
    let report = report::run_pair(mutations::pair_by_id("M07").unwrap()).unwrap();
    let summary = report.summary();

    assert!(summary.contains("paths_compared=4"));
    assert!(summary.contains("paths_changed=0"));
    assert!(summary.contains("components=[Style]"));
}