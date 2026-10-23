#[test]
fn fan_in_backward_channel_conflict_resolution_is_deterministic_and_documented() {
    assert_render_trace_enabled_when_requested();
    let rendered = crate::runtime::render_diagram(
        "graph TD\nA-->B",
        crate::format::OutputFormat::Text,
        &crate::runtime::config::RenderConfig::default(),
    )
    .expect("render trace smoke should render");
    assert!(rendered.contains("A"));

    let fixture = "fan_in_backward_channel_conflict.mmd";
    let (diagram, geom) = layout_fixture_svg(fixture);
    let first = route_graph_geometry(
        &diagram,
        &geom,
        EdgeRouting::OrthogonalRoute,
        &default_proportional_text_metrics(),
    );
    let second = route_graph_geometry(
        &diagram,
        &geom,
        EdgeRouting::OrthogonalRoute,
        &default_proportional_text_metrics(),
    );

    assert_eq!(
        first.edges.len(),
        second.edges.len(),
        "routed edge count should be deterministic"
    );

    let target_rect = geom
        .nodes
        .get("B")
        .expect("fan_in_backward_channel_conflict fixture should contain node B")
        .rect;
    let conflict = first
        .edges
        .iter()
        .find(|edge| edge.from == "Loop" && edge.to == "B")
        .expect("fixture should contain Loop -> B");

    assert!(
        conflict.is_backward,
        "Loop -> B must be backward in orthogonal routing layout for this fixture"
    );

    let source_rect = geom
        .nodes
        .get("Loop")
        .expect("fan_in_backward_channel_conflict fixture should contain node Loop")
        .rect;
    let conflict_start = conflict
        .path
        .first()
        .copied()
        .expect("backward edge should have source endpoint");
    let conflict_start_face = point_on_target_face(source_rect, conflict_start);
    // With corrected reversed-chain-edge tracking, the variable spacing
    // feature no longer inflates gaps for this fixture, so the backward
    // edge departs from right face instead of top.
    assert_eq!(
        conflict_start_face, "right",
        "Loop -> B should depart from the right face (corrected spacing): start={conflict_start:?}, path={:?}",
        conflict.path
    );
    let source_face_margin = match conflict_start_face {
        "top" | "bottom" => {
            let source_left = source_rect.x;
            let source_right = source_rect.x + source_rect.width;
            (conflict_start.x - source_left).min(source_right - conflict_start.x)
        }
        "left" | "right" => {
            let source_top = source_rect.y;
            let source_bottom = source_rect.y + source_rect.height;
            (conflict_start.y - source_top).min(source_bottom - conflict_start.y)
        }
        _ => 0.0,
    };
    assert!(
        source_face_margin >= 5.0,
        "Loop -> B source departure should stay away from source face borders (closer to center) to avoid cramped hooks: margin={source_face_margin}, source_rect={source_rect:?}, start={conflict_start:?}, path={:?}",
        conflict.path
    );
    let conflict_next = conflict
        .path
        .get(1)
        .copied()
        .expect("backward edge should have source support point");
    assert!(
        source_support_is_normal_to_attached_rect_face(source_rect, conflict_start, conflict_next),
        "Loop -> B should leave the canonical source face on its outward normal axis: start={conflict_start:?}, next={conflict_next:?}, path={:?}",
        conflict.path
    );

    let conflict_end = *conflict
        .path
        .last()
        .expect("backward edge should have path endpoint");
    let conflict_face = point_on_target_face(target_rect, conflict_end);
    assert_eq!(
        conflict_face,
        "right",
        "Loop -> B should enter B on the right face (corrected spacing): end={conflict_end:?}, path={path:?}",
        conflict_end = conflict_end,
        path = conflict.path
    );
    let conflict_prev = conflict
        .path
        .get(conflict.path.len().saturating_sub(2))
        .copied()
        .expect("backward edge should have terminal support point");
    assert!(
        terminal_support_is_normal_to_attached_rect_face(target_rect, conflict_prev, conflict_end),
        "Loop -> B should approach the canonical right face with a face-normal terminal segment: prev={conflict_prev:?}, end={conflict_end:?}, path={:?}",
        conflict.path
    );

    let incoming_to_b: Vec<_> = first.edges.iter().filter(|edge| edge.to == "B").collect();
    for edge in &incoming_to_b {
        let end = edge
            .path
            .last()
            .copied()
            .expect("inbound edge should have endpoint");
        let end_face = point_on_target_face(target_rect, end);
        tracing::trace!(
            target: "mmdflux::tests::graph_routing_pipeline",
            event = "fan_in_edge",
            source_node = %edge.from,
            target_node = %edge.to,
            edge_index = edge.index,
            is_backward = edge.is_backward,
            ?end,
            end_face = ?end_face,
        );
    }
    assert_eq!(
        incoming_to_b.len(),
        6,
        "fan_in_backward_channel_conflict should create exactly six inbound edges to B"
    );

    let right_face_count = incoming_to_b
        .iter()
        .filter(|edge| {
            let end = edge
                .path
                .last()
                .copied()
                .expect("inbound edge should have endpoint");
            point_on_target_face(target_rect, end) == "right"
        })
        .count();

    // With corrected reversed-chain-edge tracking, the backward Loop->B edge
    // now routes via the right face (reduced spacing from the fix), so exactly
    // 1 inbound edge arrives on B's right face.
    assert_eq!(
        right_face_count, 1,
        "Loop->B backward edge should arrive on B's right face with corrected spacing: right_face_count={right_face_count}"
    );
}