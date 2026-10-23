fn sequence_rect_par_over_nested_control_blocks_render() {
    let cases = [
        (
            "rect contains par_over",
            "rect rgba(0,0,0,0.1)",
            "par_over Everyone",
            "sequenceDiagram\nparticipant A\nparticipant B\nrect rgba(0,0,0,0.1)\npar_over Everyone\nA->>B: Work\nend\nend",
        ),
        (
            "par_over contains rect",
            "par_over Everyone",
            "rect rgba(0,0,0,0.1)",
            "sequenceDiagram\nparticipant A\nparticipant B\npar_over Everyone\nrect rgba(0,0,0,0.1)\nA->>B: Work\nend\nend",
        ),
    ];

    for (name, outer, inner, input) in cases {
        let rendered = render_sequence(input, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|err| panic!("{name} should render: {err}"));

        assert!(
            rendered.contains(outer),
            "{name} should render the outer frame:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with("│ ┌") && line.contains(inner)),
            "{name} should render the inner frame inside the outer frame:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with("│ │") && line.contains("Work")),
            "{name} should keep messages inside both nested frames:\n{rendered}"
        );
    }
}