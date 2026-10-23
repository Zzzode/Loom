#[test]
fn td_backward_entry_face_followup_parity_matches_text_for_decision_and_complex() {
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

    // (fixture, from, to, expected_source_face, full_target_face, orthogonal_target_face)
    // Long backward edges (rank_span >= 6) use side-face channel routing in
    // orthogonal routing (R-BACK-7 Heuristic 4), so orthogonal target face may differ
    // from polyline routing.
    type BackwardFaceCase<'a> = (&'a str, &'a str, &'a str, Option<&'a str>, &'a str, &'a str);
    let cases: [BackwardFaceCase<'_>; 2] = [
        // D is to the right of A; parity override is bypassed to avoid crossing
        // the forward A->D edge, so both polyline and orthogonal use side-channel
        // (right-face) routing. Under plan 0145 task 1.7's padded label-dummy
        // reservations, polyline converged to the same right-face entry as
        // orthogonal (previously entered via the bottom face). Visual shape of
        // the rendered edge is unchanged; this test encodes an internal face
        // classification only.
        ("decision.mmd", "D", "A", None, "right", "right"),
        // Layout quality improvements (model order + variable spacing) shifted E
        // relative to A so the polyline backward edge now enters A from the bottom
        // face instead of the left face. The orthogonal router still uses the
        // side-channel (right-face) heuristic for long backward edges.
        ("complex.mmd", "E", "A", None, "bottom", "right"),
    ];

    for (
        fixture,
        from,
        to,
        expected_source_face,
        expected_full_target,
        expected_orthogonal_target,
    ) in cases
    {
        let input = load_fixture(fixture);
        let diagram = prepare_flowchart(&input);
        let mode = default_proportional_mode();
        let config = EngineConfig::Layered(
            crate::engines::graph::algorithms::layered::LayoutConfig::default(),
        );
        let geom = run_layered_layout(&mode, &diagram, &config).expect("layout should succeed");

        let source_rect = geom
            .nodes
            .get(from)
            .unwrap_or_else(|| panic!("fixture {fixture} should contain source node {from}"))
            .rect;
        let target_rect = geom
            .nodes
            .get(to)
            .unwrap_or_else(|| panic!("fixture {fixture} should contain target node {to}"))
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
            .find(|edge| edge.from == from && edge.to == to)
            .unwrap_or_else(|| panic!("fixture {fixture} should contain edge {from}->{to}"));
        let orthogonal_edge = orthogonal
            .edges
            .iter()
            .find(|edge| edge.from == from && edge.to == to)
            .unwrap_or_else(|| panic!("fixture {fixture} should contain edge {from}->{to}"));

        let full_start = full_edge
            .path
            .first()
            .copied()
            .expect("polyline edge should have source endpoint");
        let full_end = full_edge
            .path
            .last()
            .copied()
            .expect("polyline edge should have target endpoint");
        let orthogonal_start = orthogonal_edge
            .path
            .first()
            .copied()
            .expect("orthogonal edge should have source endpoint");
        let orthogonal_end = orthogonal_edge
            .path
            .last()
            .copied()
            .expect("orthogonal edge should have target endpoint");

        let full_source_face = point_face(source_rect, full_start);
        let full_target_face = point_face(target_rect, full_end);
        let orthogonal_source_face = point_face(source_rect, orthogonal_start);
        let orthogonal_target_face = point_face(target_rect, orthogonal_end);

        if let Some(expected_source_face) = expected_source_face {
            assert_eq!(
                full_source_face, expected_source_face,
                "fixture contract changed unexpectedly: polyline {from}->{to} should use source face {expected_source_face}; path={:?}",
                full_edge.path
            );
        }
        assert_eq!(
            full_target_face, expected_full_target,
            "fixture contract changed unexpectedly: polyline {from}->{to} should use target face {expected_full_target}; path={:?}",
            full_edge.path
        );

        if let Some(expected_source_face) = expected_source_face {
            assert_eq!(
                orthogonal_source_face, expected_source_face,
                "orthogonal {from}->{to} should match TD source-face parity with text/polyline ({expected_source_face}) for fixture {fixture}; full_path={:?}, orthogonal_path={:?}",
                full_edge.path, orthogonal_edge.path
            );
        }
        assert_eq!(
            orthogonal_target_face, expected_orthogonal_target,
            "orthogonal {from}->{to} target face should be {expected_orthogonal_target} for fixture {fixture}; full_path={:?}, orthogonal_path={:?}",
            full_edge.path, orthogonal_edge.path
        );

        // Text output is routing-independent (backward edge face differences
        // only affect SVG path geometry, not text grid). Verify flux-layered
        // renders successfully for each fixture.
        let _text = render_text_with_engine(&input, "flux-layered");
    }
}