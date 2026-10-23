fn cross_group_return_heads_enter_the_authored_member_contour() {
    for direction in ["LR", "RL", "TD", "BT"] {
        for unicode in [false, true] {
            for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
                for nesting in [1, 2] {
                    let mut options = if unicode {
                        AsciiRenderOptions::unicode()
                    } else {
                        AsciiRenderOptions::ascii()
                    };
                    options.layout_profile = profile;
                    let groups = if nesting == 1 {
                        "subgraph Outer\nA[A]\nend"
                    } else {
                        "subgraph Outer\nsubgraph Inner\nA[A]\nend\nend"
                    };
                    let source = format!("flowchart {direction}\n{groups}\nA --> B[B]\nB --> A");
                    let rendered = render(&source, options);
                    assert_heads_touch_contours(&rendered, 2);
                    assert_terminals_reach_authored_nodes(&rendered);
                }
            }
        }
    }
}