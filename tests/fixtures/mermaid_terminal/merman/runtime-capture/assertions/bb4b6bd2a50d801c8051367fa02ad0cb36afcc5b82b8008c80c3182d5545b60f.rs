fn class_local_semantic_fixture_covers_wide_members_and_relation_labels() {
    let input = read_local_semantic_fixture("class/wide_members_and_summary_labels.mmd");
    let options = AsciiRenderOptions::ascii();

    let rendered = render_class_with_grid_limit(&input, &options, 10_000)
        .expect("class diagram with wide member and relation labels should render");

    for expected in [
        "User",
        "名称",
        "Order",
        "状态🚀",
        "Audit",
        "创建🚀",
        "记录数据",
    ] {
        assert!(
            rendered.contains(expected),
            "wide class fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered
            .lines()
            .all(|line| !line.contains("<br>") || line.contains("authored(bytes=")),
        "wide class relations should expose Mermaid break syntax only inside authored framing:\n{rendered}"
    );
}