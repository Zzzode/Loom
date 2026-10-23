fn feedback_edge_preserves_diamond_vertex_and_enters_target_side() {
    let input = "flowchart TD\n A[Write code] --> B{Tests pass?}\n B -- Yes --> C[Open PR]\n B -- No --> A\n";
    for mut options in [AsciiRenderOptions::unicode(), AsciiRenderOptions::ascii()] {
        for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
            options.layout_profile = profile;
            let rendered = render_flowchart(input, &options).unwrap();
            let rows: Vec<Vec<char>> = rendered
                .lines()
                .map(|line| line.chars().collect())
                .collect();
            let diamond_row = rendered
                .lines()
                .position(|line| line.contains("Tests pass?"))
                .unwrap();
            let vertex_x = rows[diamond_row].iter().rposition(|ch| *ch == '>').unwrap();
            assert!(
                rows[diamond_row][vertex_x + 1..]
                    .iter()
                    .any(|ch| matches!(ch, '-' | '─')),
                "the side escape must connect to the preserved diamond vertex: {rendered}"
            );
            let target_row = rendered
                .lines()
                .position(|line| line.contains("Write code"))
                .unwrap();
            assert!(
                rows[target_row].iter().any(|ch| matches!(ch, '◄' | '<')),
                "the feedback head must point into the right side of its target: {rendered}"
            );
        }
    }
}