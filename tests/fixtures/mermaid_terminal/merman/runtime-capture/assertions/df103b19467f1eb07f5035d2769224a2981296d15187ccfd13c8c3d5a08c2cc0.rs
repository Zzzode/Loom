fn flowchart_local_semantic_fixture_covers_cjk_boundary_routes() {
    let input = local_semantic_input("flowchart/cjk_boundary_routes.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic CJK boundary-route flowchart fixture should render");

    for expected in [
        "入口",
        "流程中枢",
        "校验层",
        "检查",
        "解析",
        "验证",
        "发布",
        "完成",
    ] {
        assert!(
            rendered.contains(expected),
            "CJK boundary-route fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("pipe") && !rendered.contains("validate"),
        "CJK boundary-route fixture should not leak subgraph ids into ASCII output:\n{rendered}"
    );

    let line_index = |needle: &str| {
        rendered
            .lines()
            .position(|line| line.contains(needle))
            .unwrap_or_else(|| panic!("missing {needle:?} in rendered fixture:\n{rendered}"))
    };
    assert!(
        line_index("检查") <= line_index("解析"),
        "CJK entry chain should keep the first step before the second step:\n{rendered}"
    );
    assert!(
        line_index("解析") <= line_index("验证"),
        "CJK entry chain should keep the second step before the validation step:\n{rendered}"
    );
    assert!(
        line_index("验证") < line_index("发布"),
        "validation step should precede the publish step in the semantic fixture:\n{rendered}"
    );
    assert!(
        line_index("发布") < line_index("完成"),
        "success boundary route should keep the completion node after the publish step:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 10,
        "CJK boundary-route fixture should produce a non-trivial multi-line layout:\n{rendered}"
    );
}