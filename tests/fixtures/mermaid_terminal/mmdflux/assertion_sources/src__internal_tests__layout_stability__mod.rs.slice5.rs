#[test]
fn route_proportionality_report_classifies_or_data_defers_m11() {
    let report =
        proportionality::run_route_proportionality_report().expect("route proportionality report");

    let m14 = report.summary_for("M14").expect("M14 summary");
    assert_eq!(
        m14.classification,
        Some(proportionality::ProportionalityClass::Disproportionate)
    );
    assert_eq!(m14.classification_reason, None);

    let m11 = report.summary_for("M11").expect("M11 summary");
    assert_eq!(
        m11.basis,
        proportionality::InputMagnitudeBasis::EdgeInsertionContext
    );

    match m11.classification {
        Some(classification) => {
            assert!(matches!(
                classification,
                proportionality::ProportionalityClass::Proportionate
                    | proportionality::ProportionalityClass::Mixed
                    | proportionality::ProportionalityClass::Disproportionate
            ));
            assert_eq!(m11.classification_reason, None);
        }
        None => {
            let reason = m11
                .classification_reason
                .as_deref()
                .expect("data-driven deferral reason");
            assert_ne!(
                reason,
                "pure-edge-insertion controls are insufficient for honest M11 classification"
            );
            assert!(
                reason.contains("pure-edge")
                    || reason.contains("cluster")
                    || reason.contains("sub-band"),
                "{reason}"
            );
        }
    }
}