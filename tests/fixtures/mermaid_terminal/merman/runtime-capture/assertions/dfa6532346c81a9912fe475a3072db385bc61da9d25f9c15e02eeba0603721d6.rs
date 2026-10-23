fn state_local_semantic_fixture_covers_cjk_connection_lifecycle() {
    let input = read_local_semantic_fixture("state/cjk_connection_lifecycle.mmd");

    let rendered = render_state(&input, &AsciiRenderOptions::ascii())
        .expect("CJK local semantic state fixture should render");

    for expected in [
        "空闲",
        "连接中",
        "已连接",
        "断开中",
        "重连中",
        "连接",
        "成功",
        "超时",
        "达到上限",
        "完成",
    ] {
        assert!(
            rendered.contains(expected),
            "CJK state fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        first_line_index_containing(&rendered, "连接中")
            < first_line_index_containing(&rendered, "等待"),
        "CJK state fixture should keep the composite lifecycle readable:\n{rendered}"
    );
    assert!(
        first_line_index_containing(&rendered, "等待")
            < first_line_index_containing(&rendered, "认证")
            && first_line_index_containing(&rendered, "认证")
                < first_line_index_containing(&rendered, "已连接")
            && first_line_index_containing(&rendered, "已连接")
                < first_line_index_containing(&rendered, "断开中"),
        "CJK state fixture should keep the internal lifecycle progression in order:\n{rendered}"
    );
    assert!(
        rendered.lines().count() >= 7,
        "CJK state fixture should produce a multi-line layout:\n{rendered}"
    );
}