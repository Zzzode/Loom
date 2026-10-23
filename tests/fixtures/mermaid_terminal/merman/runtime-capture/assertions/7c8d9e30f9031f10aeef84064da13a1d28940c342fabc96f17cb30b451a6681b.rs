fn flowchart_horizontal_label_spacing_counts_only_occupied_marker_cells() {
    use merman_ascii::TerminalWidthProfile;

    for direction in ["LR", "RL"] {
        for (arrow, source_marker, target_marker) in [
            ("---", None, None),
            ("-->", None, Some('>')),
            ("<-->", Some('<'), Some('>')),
            ("--o", None, Some('o')),
            ("--x", None, Some('x')),
            ("o--x", Some('o'), Some('x')),
            ("o-->", Some('o'), Some('>')),
            ("x-->", Some('x'), Some('>')),
        ] {
            for label in ["odd", "even", "连接说明", "A·B"] {
                for profile in [TerminalWidthProfile::Unicode, TerminalWidthProfile::Cjk] {
                    for unicode in [false, true] {
                        for layout in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                            let options = if unicode {
                                AsciiRenderOptions::unicode()
                            } else {
                                AsciiRenderOptions::ascii()
                            }
                            .with_terminal_width_profile(profile)
                            .with_layout_profile(layout);
                            let input = format!("flowchart {direction}\nA {arrow}|{label}| B");
                            let rendered = render_flowchart(&input, &options)
                                .unwrap_or_else(|error| panic!("{input}: {error}"));
                            let row = rendered
                                .lines()
                                .find(|row| row.contains(label))
                                .expect("complete label should remain on its edge");
                            let structural_unicode =
                                unicode && profile == TerminalWidthProfile::Unicode;
                            let line = if structural_unicode { "──" } else { "--" };
                            let (before, after) = row.split_once(label).unwrap();
                            assert!(
                                before.ends_with(line) && after.starts_with(line),
                                "{input}:\n{rendered}"
                            );
                            for marker in [source_marker, target_marker].into_iter().flatten() {
                                let marker = match (marker, direction, structural_unicode) {
                                    ('>', "RL", false) | ('<', "LR", false) => '<',
                                    ('<', "RL", false) | ('>', "LR", false) => '>',
                                    ('>', "RL", true) | ('<', "LR", true) => '◄',
                                    ('<', "RL", true) | ('>', "LR", true) => '►',
                                    ('o', _, true) => '○',
                                    ('x', _, true) => '×',
                                    (marker, _, _) => marker,
                                };
                                assert!(
                                    row.contains(marker),
                                    "missing {marker}: {input}\n{rendered}"
                                );
                            }
                            let width = |text: &str| match profile {
                                TerminalWidthProfile::Unicode => UnicodeWidthStr::width(text),
                                TerminalWidthProfile::Cjk => UnicodeWidthStr::width_cjk(text),
                                _ => unreachable!("test enumerates supported width profiles"),
                            };
                            let expected = 10
                                + width(label)
                                + 4
                                + usize::from(source_marker.is_some())
                                + usize::from(target_marker.is_some());
                            assert!(
                                rendered.lines().all(|row| width(row) == expected),
                                "unexpected gap width, expected {expected}: {input}\n{rendered}"
                            );
                        }
                    }
                }
            }
        }
    }
}