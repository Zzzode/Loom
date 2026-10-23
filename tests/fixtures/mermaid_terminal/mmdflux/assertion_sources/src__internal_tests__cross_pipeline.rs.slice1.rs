#[test]
fn fan_in_backward_channel_interaction_fixture_matrix_matches_documented_policy_in_text_and_svg() {
    fn edge_path_data(svg: &str) -> Vec<String> {
        svg.lines()
            .map(str::trim)
            .filter(|line| {
                line.starts_with("<path d=\"")
                    && (line.contains("marker-end=") || line.contains("marker-start="))
            })
            .filter_map(|line| {
                let start = line.find("d=\"")?;
                let after = &line[start + 3..];
                let end = after.find('"')?;
                Some(after[..end].to_string())
            })
            .collect()
    }

    fn parse_svg_path_points(path_data: &str) -> Vec<(f64, f64)> {
        path_data
            .split_whitespace()
            .filter_map(|token| {
                let token = token.trim_start_matches(|c: char| c.is_ascii_alphabetic());
                let (x, y) = token.split_once(',')?;
                Some((x.parse::<f64>().ok()?, y.parse::<f64>().ok()?))
            })
            .collect()
    }

    fn parse_attr_f64(line: &str, attr: &str) -> Option<f64> {
        let marker = format!("{attr}=\"");
        let start = line.find(&marker)? + marker.len();
        let rest = &line[start..];
        let end = rest.find('"')?;
        rest[..end].parse::<f64>().ok()
    }

    fn node_rect_for_label(svg: &str, label: &str) -> Option<(f64, f64, f64, f64)> {
        let (text_x, text_y) = svg.lines().find_map(|line| {
            if !line.contains("<text") || !line.contains(&format!(">{label}<")) {
                return None;
            }
            Some((parse_attr_f64(line, "x")?, parse_attr_f64(line, "y")?))
        })?;

        svg.lines()
            .filter_map(|line| {
                if !line.contains("<rect ") || !line.contains("stroke=") {
                    return None;
                }
                let x = parse_attr_f64(line, "x")?;
                let y = parse_attr_f64(line, "y")?;
                let width = parse_attr_f64(line, "width")?;
                let height = parse_attr_f64(line, "height")?;
                let inside =
                    text_x >= x && text_x <= x + width && text_y >= y && text_y <= y + height;
                if inside {
                    Some((x, y, width, height))
                } else {
                    None
                }
            })
            .min_by(|a, b| (a.2 * a.3).partial_cmp(&(b.2 * b.3)).unwrap())
    }

    fn svg_point_face(rect: (f64, f64, f64, f64), point: (f64, f64)) -> &'static str {
        let eps = 0.5;
        let (x, y, w, h) = rect;
        let left = x;
        let right = x + w;
        let top = y;
        let bottom = y + h;

        let on_right = (point.0 - right).abs() <= eps;
        let on_left = (point.0 - left).abs() <= eps;
        let on_top = (point.1 - top).abs() <= eps;
        let on_bottom = (point.1 - bottom).abs() <= eps;

        if on_right && point.1 > top + eps && point.1 < bottom - eps {
            "right"
        } else if on_left && point.1 > top + eps && point.1 < bottom - eps {
            "left"
        } else if on_top && point.0 > left + eps && point.0 < right - eps {
            "top"
        } else if on_bottom && point.0 > left + eps && point.0 < right - eps {
            "bottom"
        } else if on_right {
            "right"
        } else if on_left {
            "left"
        } else {
            "interior_or_corner"
        }
    }

    fn svg_terminal_approach_face(
        rect: (f64, f64, f64, f64),
        points: &[(f64, f64)],
    ) -> &'static str {
        if points.is_empty() {
            return "interior_or_corner";
        }
        let end = *points.last().expect("path should include endpoint");
        let direct_face = svg_point_face(rect, end);
        if direct_face != "interior_or_corner" {
            return direct_face;
        }
        if points.len() < 2 {
            return direct_face;
        }
        let prev = points[points.len() - 2];
        let dx = end.0 - prev.0;
        let dy = end.1 - prev.1;
        let (x, y, w, h) = rect;
        let left = x;
        let right = x + w;
        let top = y;
        let bottom = y + h;
        const MARKER_PULLBACK_TOLERANCE: f64 = 6.0;

        // SVG marker pullback can place the endpoint slightly outside the
        // node border while still visually attaching to that face.
        if end.0 > right
            && end.0 - right <= MARKER_PULLBACK_TOLERANCE
            && end.1 >= top - MARKER_PULLBACK_TOLERANCE
            && end.1 <= bottom + MARKER_PULLBACK_TOLERANCE
            && dy.abs() <= 0.5
            && dx < 0.0
        {
            return "right";
        }
        if end.0 < left
            && left - end.0 <= MARKER_PULLBACK_TOLERANCE
            && end.1 >= top - MARKER_PULLBACK_TOLERANCE
            && end.1 <= bottom + MARKER_PULLBACK_TOLERANCE
            && dy.abs() <= 0.5
            && dx > 0.0
        {
            return "left";
        }
        if end.1 > bottom
            && end.1 - bottom <= MARKER_PULLBACK_TOLERANCE
            && end.0 >= left - MARKER_PULLBACK_TOLERANCE
            && end.0 <= right + MARKER_PULLBACK_TOLERANCE
            && dx.abs() <= 0.5
            && dy < 0.0
        {
            return "bottom";
        }
        if end.1 < top
            && top - end.1 <= MARKER_PULLBACK_TOLERANCE
            && end.0 >= left - MARKER_PULLBACK_TOLERANCE
            && end.0 <= right + MARKER_PULLBACK_TOLERANCE
            && dx.abs() <= 0.5
            && dy > 0.0
        {
            return "top";
        }

        if dx.abs() >= dy.abs() {
            if dx > 0.0 {
                "right"
            } else if dx < 0.0 {
                "left"
            } else {
                "interior_or_corner"
            }
        } else if dy > 0.0 {
            "bottom"
        } else if dy < 0.0 {
            "top"
        } else {
            "interior_or_corner"
        }
    }

    fn svg_terminal_approach_face_relaxed(
        rect: (f64, f64, f64, f64),
        points: &[(f64, f64)],
    ) -> &'static str {
        if points.is_empty() {
            return "interior_or_corner";
        }
        let end = *points.last().expect("path should include endpoint");
        let direct_face = svg_point_face(rect, end);
        if direct_face != "interior_or_corner" {
            return direct_face;
        }
        if points.len() < 2 {
            return direct_face;
        }

        let prev = points[points.len() - 2];
        let dx = end.0 - prev.0;
        let dy = end.1 - prev.1;
        let (x, y, w, h) = rect;
        let left = x;
        let right = x + w;
        let top = y;
        let bottom = y + h;
        const MARKER_PULLBACK_TOLERANCE: f64 = 6.0;

        if end.0 > right
            && end.0 - right <= MARKER_PULLBACK_TOLERANCE
            && end.1 >= top - MARKER_PULLBACK_TOLERANCE
            && end.1 <= bottom + MARKER_PULLBACK_TOLERANCE
            && dx < 0.0
        {
            return "right";
        }
        if end.0 < left
            && left - end.0 <= MARKER_PULLBACK_TOLERANCE
            && end.1 >= top - MARKER_PULLBACK_TOLERANCE
            && end.1 <= bottom + MARKER_PULLBACK_TOLERANCE
            && dx > 0.0
        {
            return "left";
        }
        if end.1 > bottom
            && end.1 - bottom <= MARKER_PULLBACK_TOLERANCE
            && end.0 >= left - MARKER_PULLBACK_TOLERANCE
            && end.0 <= right + MARKER_PULLBACK_TOLERANCE
            && dy < 0.0
        {
            return "bottom";
        }
        if end.1 < top
            && top - end.1 <= MARKER_PULLBACK_TOLERANCE
            && end.0 >= left - MARKER_PULLBACK_TOLERANCE
            && end.0 <= right + MARKER_PULLBACK_TOLERANCE
            && dy > 0.0
        {
            return "top";
        }

        svg_terminal_approach_face(rect, points)
    }

    fn svg_source_departure_face(
        rect: (f64, f64, f64, f64),
        points: &[(f64, f64)],
    ) -> &'static str {
        if points.is_empty() {
            return "interior_or_corner";
        }
        let start = points[0];
        let direct_face = svg_point_face(rect, start);
        if direct_face != "interior_or_corner" {
            return direct_face;
        }
        if points.len() < 2 {
            return direct_face;
        }

        let next = points[1];
        let dx = next.0 - start.0;
        let dy = next.1 - start.1;
        if dx.abs() >= dy.abs() {
            if dx > 0.0 {
                "right"
            } else if dx < 0.0 {
                "left"
            } else {
                "interior_or_corner"
            }
        } else if dy > 0.0 {
            "bottom"
        } else if dy < 0.0 {
            "top"
        } else {
            "interior_or_corner"
        }
    }

    fn edge_path_for_svg_order(diagram: &Graph, svg: &str, edge_index: usize) -> Vec<(f64, f64)> {
        let mut visible_edge_indexes: Vec<usize> = diagram
            .edges
            .iter()
            .filter(|edge| edge.stroke != crate::graph::Stroke::Invisible)
            .map(|edge| edge.index)
            .collect();
        visible_edge_indexes.sort_unstable();

        let svg_position = visible_edge_indexes
            .iter()
            .position(|idx| *idx == edge_index)
            .expect("edge index should be visible in SVG");
        let paths = edge_path_data(svg);
        parse_svg_path_points(
            paths
                .get(svg_position)
                .expect("edge path should exist at visible edge position"),
        )
    }

    let render_with_registry = |fixture_name: &str, format: OutputFormat| {
        let input = load_fixture(fixture_name);
        crate::render_diagram(
            &input,
            format,
            &RenderConfig {
                layout_engine: Some(EngineAlgorithmId::parse("flux-layered").unwrap()),
                ..RenderConfig::default()
            },
        )
        .expect("render should succeed")
    };

    let fan_in_cases = [
        ("stacked_fan_in.mmd", "C", "Bot", 0usize),
        ("fan_in.mmd", "D", "Target", 0usize),
        ("five_fan_in.mmd", "F", "Target", 0usize),
    ];
    for (fixture_name, target_id, target_label, min_side_faces) in fan_in_cases {
        let diagram = parse_and_build(fixture_name);
        let text = render_with_registry(fixture_name, OutputFormat::Text);
        assert!(
            text.contains(target_label),
            "text output should contain target label {target_label} for {fixture_name}"
        );
        let svg = render_with_registry(fixture_name, OutputFormat::Svg);
        let rect = node_rect_for_label(&svg, target_label)
            .unwrap_or_else(|| panic!("missing target rect for {target_label} in {fixture_name}"));
        let inbound_indices: Vec<usize> = diagram
            .edges
            .iter()
            .filter(|edge| edge.to == target_id)
            .map(|edge| edge.index)
            .collect();
        assert!(
            !inbound_indices.is_empty(),
            "fixture {fixture_name} should have inbound edges to {target_id}"
        );

        let mut side_face_count = 0usize;
        let mut interior_or_corner_count = 0usize;
        for edge_index in inbound_indices {
            let points = edge_path_for_svg_order(&diagram, &svg, edge_index);
            let face = svg_terminal_approach_face(rect, &points);
            if face == "interior_or_corner" {
                interior_or_corner_count += 1;
            }
            if matches!(face, "left" | "right") {
                side_face_count += 1;
            }
        }

        assert_eq!(
            interior_or_corner_count, 0,
            "fixture {fixture_name} should keep inbound endpoints on a concrete target face under Fan-in overflow policy"
        );
        if min_side_faces == 0 {
            assert_eq!(
                side_face_count, 0,
                "fixture {fixture_name} should stay on primary TD incoming face when overflow is not required"
            );
        } else {
            assert!(
                side_face_count >= min_side_faces,
                "fixture {fixture_name} should spill overflow arrivals to side faces under Fan-in overflow policy: expected >= {min_side_faces}, actual={side_face_count}"
            );
        }
    }

    // With per-edge label spacing, unlabeled backward edges are more compact.
    // Most backward edges still use side-channel (right-face) routing, but
    // the fan_in_backward_channel_conflict case uses top-face source routing
    // (departing toward the target above) and bottom-face target arrival.
    let backward_channel_cases = [
        (
            "simple_cycle.mmd",
            "C",
            "A",
            "End",
            "Start",
            "right",
            "right",
        ),
        (
            "multiple_cycles.mmd",
            "C",
            "A",
            "Bottom",
            "Top",
            "right",
            "right",
        ),
        (
            "fan_in_backward_channel_conflict.mmd",
            "Loop",
            "B",
            "Sink",
            "Target",
            "top",
            "bottom",
        ),
        (
            "http_request.mmd",
            "Response",
            "Client",
            "Send Response",
            "Client",
            "right",
            "right",
        ),
        (
            "git_workflow.mmd",
            "Remote",
            "Working",
            "Remote Repo",
            "Working Dir",
            "bottom",
            "bottom",
        ),
    ];
    for (
        fixture_name,
        from,
        to,
        source_label,
        target_label,
        expected_source_face,
        expected_target_face,
    ) in backward_channel_cases
    {
        let diagram = parse_and_build(fixture_name);
        let text = render_with_registry(fixture_name, OutputFormat::Text);
        assert!(
            text.contains(target_label),
            "text output should contain target label {target_label} for {fixture_name}"
        );
        let svg = render_with_registry(fixture_name, OutputFormat::Svg);
        let source_rect = node_rect_for_label(&svg, source_label)
            .unwrap_or_else(|| panic!("missing source rect for {source_label} in {fixture_name}"));
        let target_rect = node_rect_for_label(&svg, target_label)
            .unwrap_or_else(|| panic!("missing target rect for {target_label} in {fixture_name}"));
        let edge_index = diagram
            .edges
            .iter()
            .find(|edge| edge.from == from && edge.to == to)
            .unwrap_or_else(|| panic!("expected edge {from} -> {to} in {fixture_name}"))
            .index;
        let points = edge_path_for_svg_order(&diagram, &svg, edge_index);
        let source_face = svg_source_departure_face(source_rect, &points);
        assert_eq!(
            source_face, expected_source_face,
            "fixture {fixture_name} edge {from}->{to} should keep expected backward source face {expected_source_face}; points={points:?}"
        );
        let target_face = svg_terminal_approach_face_relaxed(target_rect, &points);
        assert_eq!(
            target_face, expected_target_face,
            "fixture {fixture_name} edge {from}->{to} should keep expected backward target face {expected_target_face}; points={points:?}"
        );
    }
}