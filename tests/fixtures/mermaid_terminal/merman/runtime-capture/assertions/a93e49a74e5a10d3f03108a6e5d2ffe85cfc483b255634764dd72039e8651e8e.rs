fn compact_and_auto_keep_sibling_group_frames_separate_at_width_fifty() {
    let model = parse_model(
        "flowchart LR\n subgraph CI\n L[Lint] --> T[Test]\n end\n subgraph Deploy\n S[Staging] --> P[Prod]\n end\n T --> S\n",
    );
    for options in [AsciiRenderOptions::unicode(), AsciiRenderOptions::ascii()] {
        let compact = render_model_report(
            &model,
            &options.with_layout_profile(AsciiLayoutProfile::Compact),
            AsciiViewportPolicy::with_max_width(50),
        )
        .unwrap();
        assert_eq!(compact.primary_extent.width, 50);
        assert_eq!(compact.primary_extent.height, 11);
        let automatic = render_model_report(
            &model,
            &options.with_layout_profile(AsciiLayoutProfile::Auto),
            AsciiViewportPolicy::with_max_width(50),
        )
        .unwrap();
        assert_eq!(automatic.layout_profile, AsciiLayoutProfile::Compact);
        assert_eq!(automatic.text, compact.text);
        let top: Vec<char> = compact.text.lines().next().unwrap().chars().collect();
        let bottom: Vec<char> = compact.text.lines().last().unwrap().chars().collect();
        let (left, right) = if options == AsciiRenderOptions::unicode() {
            ('┌', '┐')
        } else {
            ('+', '+')
        };
        let corners: Vec<_> = top
            .iter()
            .enumerate()
            .filter(|(_, ch)| **ch == left || **ch == right)
            .map(|(index, _)| index)
            .collect();
        assert_eq!(
            corners.len(),
            4,
            "two independent closed frames: {}",
            compact.text
        );
        assert_eq!(
            corners[2],
            corners[1] + 2,
            "one column must separate the frames"
        );
        assert_eq!(top[corners[1] + 1], ' ');
        assert_eq!(bottom[corners[1] + 1], ' ');
        for label in ["CI", "Deploy", "Lint", "Test", "Staging", "Prod"] {
            assert_eq!(compact.text.matches(label).count(), 1);
        }
    }
}