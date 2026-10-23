fn leaning_terminals_and_multiline_labels_share_the_actual_contour() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for unicode in [false, true] {
            for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                for padding in [None, Some((0, 0)), Some((1, 0))] {
                    for shape in ["lean-r", "lean-l"] {
                        for leaning_source in [false, true] {
                            for edge in ["-->", "---", "-.->", "==>"] {
                                let mut options = if unicode {
                                    AsciiRenderOptions::unicode()
                                } else {
                                    AsciiRenderOptions::ascii()
                                };
                                options.layout_profile = profile;
                                if let Some((x, y)) = padding {
                                    options.node_padding_x = Some(x);
                                    options.node_padding_y = Some(y);
                                }
                                let lean = format!(
                                    "A@{{ shape: {shape}, label: \"aaa<br/>bbb<br/>ccc\" }}"
                                );
                                let source = if leaning_source {
                                    format!("flowchart {direction}\n{lean} {edge} B[B]")
                                } else {
                                    format!("flowchart {direction}\nB[B] {edge} {lean}")
                                };
                                let rendered = render(&source, options);
                                for text in ["aaa", "bbb", "ccc"] {
                                    let row = rendered
                                        .lines()
                                        .find(|row| row.contains(text))
                                        .unwrap_or_else(|| {
                                            panic!("{text} must survive:\n{rendered}")
                                        });
                                    let text_x = row.find(text).unwrap();
                                    assert!(
                                        row[..text_x].contains(['/', '\\'])
                                            && row[text_x + text.len()..].contains(['/', '\\']),
                                        "each multiline label must stay between its own row's sloping sides:\n{rendered}"
                                    );
                                }
                                assert_heads_touch_contours(&rendered, usize::from(edge != "---"));
                            }
                        }
                    }
                }
            }
        }
    }
}