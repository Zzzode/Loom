#[test]
fn m14_route_trace_includes_label_lane_snapshot() {
    let pair = mutations::pair_by_id("M14").unwrap();
    let rendered = render_surfaces::render_pair(pair).unwrap();

    let snapshot = rendered
        .after
        .route_trace
        .label_lanes()
        .expect("M14 should capture label-lane stage");

    assert!(!snapshot.edges.is_empty(), "{snapshot:?}");
    assert!(!snapshot.compartments.is_empty(), "{snapshot:?}");
    assert!(!snapshot.subclusters.is_empty(), "{snapshot:?}");
    assert!(
        snapshot
            .edges
            .iter()
            .all(|edge| edge.mmds_edge_id == format!("e{}", edge.edge_index)),
        "{snapshot:?}"
    );
}