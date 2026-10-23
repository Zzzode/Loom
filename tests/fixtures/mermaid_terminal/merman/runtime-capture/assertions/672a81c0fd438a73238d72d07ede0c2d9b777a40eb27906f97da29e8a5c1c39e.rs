fn zero_horizontal_padding_preserves_subroutine_inner_decoration() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for unicode in [false, true] {
            let mut options = if unicode {
                AsciiRenderOptions::unicode()
            } else {
                AsciiRenderOptions::ascii()
            };
            options.node_padding_x = Some(0);
            options.node_padding_y = Some(0);
            let rendered = render(&format!("flowchart {direction}\nA[[AB]]"), options);
            let row = rendered.lines().find(|row| row.contains("AB")).unwrap();
            assert_eq!(
                row.chars().filter(|ch| matches!(ch, '|' | '│')).count(),
                4,
                "intrinsic inner bars must survive independently of configurable whitespace:\n{rendered}"
            );
        }
    }
}