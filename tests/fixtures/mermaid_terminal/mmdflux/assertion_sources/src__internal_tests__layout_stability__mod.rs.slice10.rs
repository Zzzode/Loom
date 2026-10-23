#[test]
fn m06_report_includes_phase_attribution() {
    let report = report::run_pair(mutations::pair_by_id("M06").unwrap()).unwrap();
    let summary = report.summary();

    assert!(summary.contains("first_divergence="));
    assert!(summary.contains("phase_components="));
    assert!(summary.contains("paths_changed=1"));
}