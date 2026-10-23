fn diamond_self_loop_source_head_keeps_its_straight_berth_and_bend() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for (options, left_head, right_head, horizontal) in [
            (AsciiRenderOptions::ascii(), '<', '>', '-'),
            (AsciiRenderOptions::unicode(), '◄', '►', '─'),
        ] {
            for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                let rendered = render_flowchart(
                    &format!("flowchart {direction}\nA{{AAA}} <--> A"),
                    &options.with_layout_profile(profile),
                )
                .expect("a bidirectional diamond self loop should render");
                let row = rendered.lines().find(|line| line.contains("AAA")).unwrap();
                let chars = row.chars().collect::<Vec<_>>();
                let label_start = chars.windows(3).position(|part| part == ['A'; 3]).unwrap();
                let vertex = if direction == "RL" {
                    chars[..label_start]
                        .iter()
                        .rposition(|ch| *ch == '<')
                        .unwrap()
                } else {
                    label_start
                        + 3
                        + chars[label_start + 3..]
                            .iter()
                            .position(|ch| *ch == '>')
                            .unwrap()
                };
                let head = if direction == "RL" {
                    vertex - 1
                } else {
                    vertex + 1
                };
                assert_eq!(
                    chars[head],
                    if direction == "RL" {
                        right_head
                    } else {
                        left_head
                    },
                    "{rendered}"
                );
                let after_head = if direction == "RL" {
                    &chars[..head]
                } else {
                    &chars[head + 1..]
                };
                assert!(
                    after_head
                        .iter()
                        .any(|ch| matches!(ch, '+' | '┌' | '┐' | '└' | '┘')),
                    "the source marker must leave the exterior bend intact: {rendered}"
                );
                assert!(
                    after_head.iter().all(|ch| *ch == ' '
                        || *ch == horizontal
                        || matches!(ch, '+' | '┌' | '┐' | '└' | '┘')),
                    "source head must remain on the straight terminal run: {rendered}"
                );
            }
        }
    }
}