fn nested_empty_sibling_frames_are_closed_and_keep_a_reading_gutter() {
    fn frame_for_title(rows: &[Vec<char>], title: &str) -> (usize, usize, usize, usize) {
        let title_chars = title.chars().collect::<Vec<_>>();
        let (title_y, title_x) = rows
            .iter()
            .enumerate()
            .find_map(|(y, row)| {
                row.windows(title_chars.len())
                    .position(|part| part == title_chars)
                    .map(|x| (y, x))
            })
            .expect("authored group title must survive");
        // BT mirrors the title row toward the bottom. Locate the closed enclosing frame
        // rather than assuming that a title always appears directly below its top border.
        for top in (0..title_y).rev() {
            let Some(left) = (0..=title_x).rev().find(|x| rows[top][*x] == '┌') else {
                continue;
            };
            let Some(right) =
                (title_x + title_chars.len()..rows[top].len()).find(|x| rows[top][*x] == '┐')
            else {
                continue;
            };
            let Some(bottom) =
                (title_y + 1..rows.len()).find(|y| rows[*y][left] == '└' && rows[*y][right] == '┘')
            else {
                continue;
            };
            for border in [&rows[top], &rows[bottom]] {
                assert!(
                    border[left + 1..right]
                        .iter()
                        .all(|ch| matches!(ch, '─' | '┼' | '┬' | '┴')),
                    "frame must retain its horizontal connections"
                );
            }
            return (left, top, right, bottom);
        }
        panic!("complete frame around title {title} must survive");
    }
    for direction in ["LR", "RL", "TD", "BT"] {
        for recursive_empty in [false, true] {
            for empty_first in [false, true] {
                let nested = if recursive_empty {
                    "subgraph F\nend\n"
                } else {
                    ""
                };
                let populated_source = format!("subgraph X\nA[AAA]\n{nested}end\n");
                let empty_source = "subgraph E\nend\n";
                let children = if empty_first {
                    format!("{empty_source}{populated_source}")
                } else {
                    format!("{populated_source}{empty_source}")
                };
                let source = format!(
                    "flowchart {direction}
subgraph G1
{children}end
subgraph G2
B[BBB]
end
A --> B
"
                );
                let model = parse_model(&source);
                for layout_profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                    let text = render_model(
                        &model,
                        &AsciiRenderOptions::unicode().with_layout_profile(layout_profile),
                    )
                    .expect("empty and populated siblings should render");
                    let rows: Vec<Vec<char>> =
                        text.lines().map(|line| line.chars().collect()).collect();
                    let populated = frame_for_title(&rows, "X");
                    let empty = frame_for_title(&rows, "E");
                    let separated = populated.2 + 1 < empty.0
                        || empty.2 + 1 < populated.0
                        || populated.3 + 1 < empty.1
                        || empty.3 + 1 < populated.1;
                    assert!(
                        separated,
                        "siblings require an empty column or row: {direction} {layout_profile:?}\n{text}"
                    );
                    if recursive_empty {
                        let nested_empty = frame_for_title(&rows, "F");
                        assert!(
                            populated.0 < nested_empty.0
                                && nested_empty.2 < populated.2
                                && populated.1 < nested_empty.1
                                && nested_empty.3 < populated.3,
                            "populated sibling must contain its recursive empty frame: {direction} {layout_profile:?}\n{text}"
                        );
                    }
                    let (left, top, right, bottom) = frame_for_title(&rows, "G1");
                    for child in [populated, empty] {
                        assert!(
                            left < child.0 && child.2 < right && top < child.1 && child.3 < bottom,
                            "parent must contain complete child frames: {direction} {layout_profile:?}\n{text}"
                        );
                    }
                }
            }
        }
    }
}