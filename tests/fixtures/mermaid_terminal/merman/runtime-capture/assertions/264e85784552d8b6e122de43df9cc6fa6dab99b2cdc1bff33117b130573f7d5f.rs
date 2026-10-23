fn separate_empty_subtrees_keep_all_titles_and_complete_frames_in_every_scope() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for reverse in [false, true] {
            for deep in [false, true] {
                let nested = if deep { "subgraph D1\nend\n" } else { "" };
                let first = format!("subgraph P1\nsubgraph E1\n{nested}end\nend\n");
                let second = "subgraph P2\nsubgraph E2\nend\nend\n";
                for external in [false, true] {
                    let groups = if reverse {
                        format!("{second}{first}")
                    } else {
                        format!("{first}{second}")
                    };
                    let outside = if external { "C[CCCC]\n" } else { "" };
                    let source = format!("flowchart {direction}\n{groups}{outside}");
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
                                .expect("empty compound subtrees must render");
                            let rows = text
                                .lines()
                                .map(|line| line.chars().collect())
                                .collect::<Vec<Vec<char>>>();
                            let context = format!(
                                "{direction} {profile:?}, reverse={reverse}, deep={deep}, external={external}, zero={zero_padding}\n{text}"
                            );
                            let p1 = frame_for_label(&rows, "P1", &context);
                            let e1 = frame_for_label(&rows, "E1", &context);
                            let p2 = frame_for_label(&rows, "P2", &context);
                            let e2 = frame_for_label(&rows, "E2", &context);
                            assert_contains(p1, e1, &context);
                            assert_contains(p2, e2, &context);
                            assert_gutter(p1, p2, &context);
                            if deep {
                                assert_contains(
                                    e1,
                                    frame_for_label(&rows, "D1", &context),
                                    &context,
                                );
                            }
                            if external {
                                let node = frame_for_label(&rows, "CCCC", &context);
                                assert_gutter(p1, node, &context);
                                assert_gutter(p2, node, &context);
                            }
                        }
                    }
                }
            }
        }
    }
}