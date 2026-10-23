#[test]
fn style_only_canaries_compare_paths_without_geometry_changes() {
    let m07 = report::run_pair(mutations::pair_by_id("M07").unwrap()).unwrap();
    let s03 = report::run_pair(mutations::pair_by_id("S03").unwrap()).unwrap();

    assert_eq!(m07.routed.path_metrics.len(), 4);
    assert_eq!(s03.routed.path_metrics.len(), 2);
    assert_eq!(m07.routed.changed_path_geometry_count(), 0);
    assert_eq!(s03.routed.changed_path_geometry_count(), 0);
}