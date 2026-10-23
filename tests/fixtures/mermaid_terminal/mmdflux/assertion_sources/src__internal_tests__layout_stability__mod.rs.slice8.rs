#[test]
fn plan_0166_guardrails_preserve_prior_calibration_outputs() {
    let report =
        proportionality::run_route_proportionality_report().expect("route proportionality report");

    let m14 = report.summary_for("M14").expect("M14 summary");
    assert_eq!(
        m14.classification,
        Some(proportionality::ProportionalityClass::Disproportionate)
    );
    assert!((m14.aggregate_ratio.expect("M14 aggregate ratio") - 4.5881).abs() < 0.25);
    assert_eq!(m14.additive_path_shape_count, Some(15));
    assert_eq!(m14.label_geometry_overlap_count, Some(6));

    for (id, expected_p75) in [("M02", 78.13), ("M03", 78.81), ("M04", 13.37)] {
        let summary = report.summary_for(id).expect("control summary");
        assert!(
            (summary.output.p75_abs_route_len_delta - expected_p75).abs() < 1.0,
            "{id} p75 changed: {}",
            summary.output.p75_abs_route_len_delta
        );
    }

    for id in ["M05", "M07", "S02", "S03"] {
        let pair = mutations::pair_by_id(id).expect("guardrail pair");
        let pair_report = report::run_pair(pair).expect("guardrail report");
        assert_eq!(pair_report.routed.changed_path_geometry_count(), 0, "{id}");
    }
}