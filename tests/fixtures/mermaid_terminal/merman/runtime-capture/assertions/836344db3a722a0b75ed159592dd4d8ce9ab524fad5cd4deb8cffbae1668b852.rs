fn mixed_empty_member_subtrees_do_not_follow_foreign_sibling_nodes() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for reverse in [false, true] {
            for empty_first in [false, true] {
                for deep in [false, true] {
                    let nested = if deep { "subgraph Deep\nend\n" } else { "" };
                    let empty = format!("subgraph Empty\n{nested}end\n");
                    let members = if empty_first {
                        format!("{empty}A[AAA]\n")
                    } else {
                        format!("A[AAA]\n{empty}")
                    };
                    let left = format!("subgraph Left\n{members}end\n");
                    let right = "subgraph Right\nB[BBB]\nend\n";
                    let groups = if reverse {
                        format!("{right}{left}")
                    } else {
                        format!("{left}{right}")
                    };
                    let source = format!("flowchart {direction}\n{groups}C[CCCC]\n");
                    let model = parse_model(&source);
                    for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                        for zero_padding in [false, true] {
                            let mut options =
                                AsciiRenderOptions::unicode().with_layout_profile(profile);
                            if zero_padding {
                                options = options
                                    .with_node_padding_x(0)
                                    .with_node_padding_y(0)
                                    .with_graph_padding_x(0)
                                    .with_graph_padding_y(0);
                            }
                            let text = render_model(&model, &options)
                                .expect("mixed compound siblings must render");
                            let rows = text
                                .lines()
                                .map(|line| line.chars().collect())
                                .collect::<Vec<Vec<char>>>();
                            let context = format!(
                                "{direction} {profile:?}, reverse={reverse}, empty_first={empty_first}, deep={deep}, zero={zero_padding}\n{text}"
                            );
                            let left = frame_for_label(&rows, "Left", &context);
                            let right = frame_for_label(&rows, "Right", &context);
                            let empty = frame_for_label(&rows, "Empty", &context);
                            let a = frame_for_label(&rows, "AAA", &context);
                            let b = frame_for_label(&rows, "BBB", &context);
                            let c = frame_for_label(&rows, "CCCC", &context);
                            assert_contains(left, empty, &context);
                            assert_contains(left, a, &context);
                            assert_contains(right, b, &context);
                            assert_gutter(left, right, &context);
                            assert_gutter(left, c, &context);
                            assert_gutter(right, c, &context);
                            assert_gutter(empty, a, &context);
                            if deep {
                                assert_contains(
                                    empty,
                                    frame_for_label(&rows, "Deep", &context),
                                    &context,
                                );
                            }
                        }
                    }
                }
            }
        }
    }
}