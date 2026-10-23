#[test]
fn sequence_canary_never_enters_graph_stability_reports() {
    let reports = report::run_corpus(mutations::tier_a_pairs()).expect("corpus should run");

    assert!(reports.iter().all(|report| report.pair_id != "S-SEQ"));
}