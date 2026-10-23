#[test]
fn lr_backward_spacing_followup_matches_text_parity_for_git_and_http() {
    const MIN_GIT_CHANNEL_CLEARANCE: f64 = 12.0;
    const MAX_HTTP_RIGHT_CLEARANCE_SHRINK_FROM_FULL: f64 = 8.0;

    fn point_face(
        rect: crate::graph::geometry::FRect,
        point: crate::graph::geometry::FPoint,
    ) -> &'static str {
        let eps = 0.5;
        let left = rect.x;
        let right = rect.x + rect.width;
        let top = rect.y;
        let bottom = rect.y + rect.height;

        let on_right = (point.x - right).abs() <= eps;
        let on_left = (point.x - left).abs() <= eps;
        let on_top = (point.y - top).abs() <= eps;
        let on_bottom = (point.y - bottom).abs() <= eps;

        if on_right && point.y > top + eps && point.y < bottom - eps {
            "right"
        } else if on_left && point.y > top + eps && point.y < bottom - eps {
            "left"
        } else if on_top && point.x > left + eps && point.x < right - eps {
            "top"
        } else if on_bottom && point.x > left + eps && point.x < right - eps {
            "bottom"
        } else if on_right {
            "right"
        } else if on_left {
            "left"
        } else {
            "interior_or_corner"
        }
    }

    let render_text_with_engine = |input: &str, engine: &str| {
        crate::render_diagram(
            input,
            OutputFormat::Text,
            &RenderConfig {
                layout_engine: EngineAlgorithmId::parse(engine).ok(),
                ..RenderConfig::default()
            },
        )
        .expect("text render should succeed")
    };

    {
        let fixture = "git_workflow.mmd";
        let input = load_fixture(fixture);
        let diagram = prepare_flowchart(&input);
        let mode = default_proportional_mode();
        let config = EngineConfig::Layered(
            crate::engines::graph::algorithms::layered::LayoutConfig::default(),
        );
        let geom = run_layered_layout(&mode, &diagram, &config).expect("layout should succeed");
        assert_eq!(
            geom.direction,
            Direction::LeftRight,
            "fixture {fixture} should remain LR for backward channel spacing parity checks"
        );

        let source_rect = geom
            .nodes
            .get("Remote")
            .unwrap_or_else(|| panic!("fixture {fixture} should contain source node Remote"))
            .rect;
        let target_rect = geom
            .nodes
            .get("Working")
            .unwrap_or_else(|| panic!("fixture {fixture} should contain target node Working"))
            .rect;

        let full = route_graph_geometry(
            &diagram,
            &geom,
            EdgeRouting::PolylineRoute,
            &default_proportional_text_metrics(),
        );
        let orthogonal = route_graph_geometry(
            &diagram,
            &geom,
            EdgeRouting::OrthogonalRoute,
            &default_proportional_text_metrics(),
        );

        let full_edge = full
            .edges
            .iter()
            .find(|edge| edge.from == "Remote" && edge.to == "Working")
            .expect("fixture should contain edge Remote -> Working");
        let orthogonal_edge = orthogonal
            .edges
            .iter()
            .find(|edge| edge.from == "Remote" && edge.to == "Working")
            .expect("fixture should contain edge Remote -> Working");

        let full_start = full_edge.path[0];
        let _full_end = *full_edge
            .path
            .last()
            .expect("full edge should have endpoint");
        let orthogonal_start = orthogonal_edge.path[0];
        let orthogonal_end = *orthogonal_edge
            .path
            .last()
            .expect("orthogonal edge should have endpoint");
        let _full_source_face = point_face(source_rect, full_start);
        assert_eq!(
            point_face(source_rect, orthogonal_start),
            "bottom",
            "orthogonal Remote -> Working should preserve canonical bottom source face while matching spacing parity; full_path={:?}, orthogonal_path={:?}",
            full_edge.path,
            orthogonal_edge.path
        );
        assert_eq!(
            point_face(target_rect, orthogonal_end),
            "bottom",
            "orthogonal Remote -> Working should preserve canonical bottom target face while matching spacing parity; full_path={:?}, orthogonal_path={:?}",
            full_edge.path,
            orthogonal_edge.path
        );

        let node_envelope_bottom =
            (source_rect.y + source_rect.height).max(target_rect.y + target_rect.height);
        let orthogonal_lane_y = orthogonal_edge
            .path
            .iter()
            .map(|point| point.y)
            .fold(f64::NEG_INFINITY, f64::max);
        assert!(
            orthogonal_lane_y >= node_envelope_bottom + MIN_GIT_CHANNEL_CLEARANCE - 0.001,
            "orthogonal Remote -> Working channel lane should have >= {MIN_GIT_CHANNEL_CLEARANCE}px clearance from node envelope (R-BACK-8): node_envelope_bottom={node_envelope_bottom}, orthogonal_lane_y={orthogonal_lane_y}, clearance={}, full_path={:?}, orthogonal_path={:?}",
            orthogonal_lane_y - node_envelope_bottom,
            full_edge.path,
            orthogonal_edge.path
        );

        // Verify flux-layered text renders successfully for this fixture.
        let _text = render_text_with_engine(&input, "flux-layered");
    }

    {
        let fixture = "http_request.mmd";
        let input = load_fixture(fixture);
        let diagram = prepare_flowchart(&input);
        let config = EngineConfig::Layered(
            crate::engines::graph::algorithms::layered::LayoutConfig::default(),
        );
        let geom = run_layered_layout(&MeasurementMode::Grid, &diagram, &config)
            .expect("layout should succeed");

        let source_rect = geom
            .nodes
            .get("Response")
            .unwrap_or_else(|| panic!("fixture {fixture} should contain source node Response"))
            .rect;
        let target_rect = geom
            .nodes
            .get("Client")
            .unwrap_or_else(|| panic!("fixture {fixture} should contain target node Client"))
            .rect;

        let full = route_graph_geometry(
            &diagram,
            &geom,
            EdgeRouting::PolylineRoute,
            &default_proportional_text_metrics(),
        );
        let orthogonal = route_graph_geometry(
            &diagram,
            &geom,
            EdgeRouting::OrthogonalRoute,
            &default_proportional_text_metrics(),
        );

        let full_edge = full
            .edges
            .iter()
            .find(|edge| edge.from == "Response" && edge.to == "Client")
            .expect("fixture should contain edge Response -> Client");
        let orthogonal_edge = orthogonal
            .edges
            .iter()
            .find(|edge| edge.from == "Response" && edge.to == "Client")
            .expect("fixture should contain edge Response -> Client");

        let full_start = full_edge.path[0];
        let _full_end = *full_edge
            .path
            .last()
            .expect("full edge should have endpoint");
        let orthogonal_start = orthogonal_edge.path[0];
        let orthogonal_end = *orthogonal_edge
            .path
            .last()
            .expect("orthogonal edge should have endpoint");
        let _full_source_face = point_face(source_rect, full_start);
        assert_eq!(
            point_face(source_rect, orthogonal_start),
            "right",
            "orthogonal Response -> Client should preserve canonical right source face while matching right-clearance parity; full_path={:?}, orthogonal_path={:?}",
            full_edge.path,
            orthogonal_edge.path
        );
        assert_eq!(
            point_face(target_rect, orthogonal_end),
            "right",
            "orthogonal Response -> Client should preserve canonical right target face while matching right-clearance parity; full_path={:?}, orthogonal_path={:?}",
            full_edge.path,
            orthogonal_edge.path
        );

        let full_right_lane_x = full_edge
            .path
            .iter()
            .map(|point| point.x)
            .fold(f64::NEG_INFINITY, f64::max);
        let orthogonal_right_lane_x = orthogonal_edge
            .path
            .iter()
            .map(|point| point.x)
            .fold(f64::NEG_INFINITY, f64::max);
        assert!(
            orthogonal_right_lane_x + MAX_HTTP_RIGHT_CLEARANCE_SHRINK_FROM_FULL
                >= full_right_lane_x,
            "orthogonal Response -> Client should preserve right-side clearance close to polyline text baseline (allowed shrink <= {MAX_HTTP_RIGHT_CLEARANCE_SHRINK_FROM_FULL}): full_right_lane_x={full_right_lane_x}, orthogonal_right_lane_x={orthogonal_right_lane_x}, full_path={:?}, orthogonal_path={:?}",
            full_edge.path,
            orthogonal_edge.path
        );

        // Verify flux-layered text renders successfully for this fixture.
        let _text = render_text_with_engine(&input, "flux-layered");
    }
}