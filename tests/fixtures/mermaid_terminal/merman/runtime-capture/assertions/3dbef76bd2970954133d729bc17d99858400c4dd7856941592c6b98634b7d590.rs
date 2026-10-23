fn compact_flowchart_route_and_group_corpus_preserves_authored_fields() {
    let cases: [(&str, &[&str]); 9] = [
        (
            "flowchart/ampersand_fanin_fanout.mmd",
            &[
                "SourceA", "SourceB", "Merge", "Fanout", "TargetA", "TargetB",
            ],
        ),
        (
            "flowchart/back_edge_labels.mmd",
            &["back to top", "back to middle"],
        ),
        (
            "flowchart/boundary_label_lane.mmd",
            &["Pipeline", "boundaryLabelWithEnoughWidth", "Success"],
        ),
        (
            "flowchart/cjk_boundary_routes.mmd",
            &["入口", "流程中枢", "校验层", "验证", "发布", "完成"],
        ),
        (
            "flowchart/disconnected_subgraphs.mmd",
            &["Today", "Today Markdown", "Next Wave", "Next Widget"],
        ),
        (
            "flowchart/multi_boundary_routes.mmd",
            &["load", "check", "Pipeline", "ok", "fail", "Retry"],
        ),
        ("flowchart/multiline_edge_label.mmd", &["north", "south"]),
        (
            "flowchart/nested_direction_boundary.mmd",
            &[
                "Outer Pipeline",
                "Inner Steps",
                "Validate",
                "Persist",
                "Done",
            ],
        ),
        (
            "flowchart/sibling_boundary_routes.mmd",
            &["Left Group", "Right Group", "Alpha", "Delta", "handoff"],
        ),
    ];
    let profiles = [
        AsciiRenderOptions::ascii(),
        AsciiRenderOptions::unicode(),
        AsciiRenderOptions::ascii().with_terminal_width_profile(TerminalWidthProfile::Cjk),
        AsciiRenderOptions::unicode().with_terminal_width_profile(TerminalWidthProfile::Cjk),
    ];

    for (fixture, expected_fields) in cases {
        let model = parse_model(&local_semantic_input(fixture));
        for options in profiles {
            let options = options.with_layout_profile(AsciiLayoutProfile::Compact);
            let rendered = render_model(&model, &options).unwrap_or_else(|error| {
                panic!("compact {fixture} render failed for {options:?}: {error}")
            });
            assert_rectangular_terminal_grid_with_profile(
                &rendered,
                options.terminal_width_profile,
            );
            for field in expected_fields {
                assert!(
                    rendered.contains(field),
                    "compact {fixture} lost authored field {field:?}:\n{rendered}"
                );
            }
        }
    }
}