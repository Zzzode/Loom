#[test]
fn corpus_reports_have_positive_signal_for_known_mutations() {
    let reports = report::run_corpus(mutations::tier_a_pairs()).expect("corpus should run");

    let m06 = reports
        .iter()
        .find(|report| report.pair_id == "M06")
        .unwrap();
    assert!(
        !m06.routed.label_rect_changes.is_empty(),
        "M06 should exercise label geometry"
    );

    let m10 = reports
        .iter()
        .find(|report| report.pair_id == "M10")
        .unwrap();
    assert!(
        !m10.routed.endpoint_face_changes.is_empty() || m10.routed.routed_bounds_delta.changed,
        "M10 should produce routed endpoint or bounds signal"
    );

    let m15 = reports
        .iter()
        .find(|report| report.pair_id == "M15")
        .unwrap();
    assert!(
        !m15.quality.label_path_drift.is_empty() || !m15.quality.label_rect_overlaps.is_empty(),
        "M15 should exercise label drift or overlap oracle signal"
    );
}