fn fan_in_routes_keep_five_independent_heads_and_continuous_target_terminals() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for unicode in [false, true] {
            for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                for padding_y in [3, 5] {
                    let mut options = if unicode {
                        AsciiRenderOptions::unicode()
                    } else {
                        AsciiRenderOptions::ascii()
                    };
                    options.layout_profile = profile;
                    options.graph_padding_x = 5;
                    options.graph_padding_y = padding_y;
                    let source = format!(
                        "flowchart {direction}\nA --> B\nB --> C\nA --> C\nB --> D\nD --> C"
                    );
                    let rendered = render(&source, options);
                    let rows: Vec<Vec<char>> =
                        rendered.lines().map(|row| row.chars().collect()).collect();
                    let cell = |x: isize, y: isize| {
                        usize::try_from(y)
                            .ok()
                            .and_then(|y| rows.get(y))
                            .and_then(|row| usize::try_from(x).ok().and_then(|x| row.get(x)))
                            .copied()
                            .unwrap_or(' ')
                    };
                    let boxes = ['A', 'B', 'C', 'D'].map(|label| {
                        let (left, right, top, bottom) = node_frame(&rows, label);
                        (label, left, right, top, bottom)
                    });
                    let mut targets = Vec::new();
                    for (y, row) in rows.iter().enumerate() {
                        for (x, head) in row.iter().copied().enumerate() {
                            let (dx, dy, axis) = match head {
                                '>' | '►' => (1, 0, 2 | 8),
                                '<' | '◄' => (-1, 0, 2 | 8),
                                '^' | '▲' => (0, -1, 1 | 4),
                                'v' | '▼' => (0, 1, 1 | 4),
                                _ => continue,
                            };
                            let (mut terminal_x, mut terminal_y) = (x as isize, y as isize);
                            let mut reached = None;
                            for _ in 0..rows.len() + row.len() {
                                terminal_x += dx;
                                terminal_y += dy;
                                if let Some((label, _, _, _, _)) =
                                    boxes.iter().find(|(_, left, right, top, bottom)| {
                                        ((*left == terminal_x || *right == terminal_x)
                                            && (*top..=*bottom).contains(&terminal_y))
                                            || ((*top == terminal_y || *bottom == terminal_y)
                                                && (*left..=*right).contains(&terminal_x))
                                    })
                                {
                                    reached = Some(*label);
                                    break;
                                }
                                let next = cell(terminal_x, terminal_y);
                                assert!(
                                    route_directions(next) & axis == axis || next == head,
                                    "each allocated head must retain the straight shared terminal into its target:\n{rendered}"
                                );
                            }
                            targets.push(reached.unwrap_or_else(|| {
                                panic!("head must reach an authored node:\n{rendered}")
                            }));
                        }
                    }
                    targets.sort_unstable();
                    assert_eq!(
                        targets,
                        ['B', 'C', 'C', 'C', 'D'],
                        "all five authored heads need independent berths into the correct targets:\n{rendered}"
                    );
                }
            }
        }
    }
}