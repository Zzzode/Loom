fn issue_53_width_matrix_preserves_semantics_and_terminal_extent() {
    let source = local_semantic_input("flowchart/issue_53_long_node_labels.mmd");
    let model = parse_model(&source);
    let layouts = [
        ("canonical", AsciiLayoutProfile::Canonical, (74, 57)),
        ("compact", AsciiLayoutProfile::Compact, (56, 67)),
    ];

    for (layout_name, layout_profile, expected_extent) in layouts {
        let profiles = [
            ("ascii-unicode", AsciiRenderOptions::ascii()),
            ("unicode-unicode", AsciiRenderOptions::unicode()),
            (
                "ascii-cjk",
                AsciiRenderOptions::ascii().with_terminal_width_profile(TerminalWidthProfile::Cjk),
            ),
            (
                "unicode-cjk",
                AsciiRenderOptions::unicode()
                    .with_terminal_width_profile(TerminalWidthProfile::Cjk),
            ),
        ];

        for (profile_name, options) in profiles {
            let options = options.with_layout_profile(layout_profile);
            let rendered = render_model(&model, &options).unwrap_or_else(|error| {
                panic!("{layout_name}/{profile_name} Issue #53 render failed: {error}")
            });
            let (width, height) =
                terminal_extent_with_profile(&rendered, options.terminal_width_profile);

            assert_issue_53_semantics(&rendered);
            assert_eq!(
                (width, height),
                expected_extent,
                "{layout_name}/{profile_name} Issue #53 extent changed"
            );
            assert_rectangular_terminal_grid_with_profile(
                &rendered,
                options.terminal_width_profile,
            );

            let fits = WIDTH_MATRIX.map(|max_width| width <= max_width);
            assert!(
                fits.windows(2).all(|window| !window[0] || window[1]),
                "width-fit classification must be monotonic for {layout_name}/{profile_name}: width={width}, fits={fits:?}"
            );
            assert!(
                fits[3],
                "the 120-cell characterization bound must contain {layout_name}/{profile_name} output"
            );
        }
    }
}