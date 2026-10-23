fn er_local_semantic_fixture_covers_wide_attributes_and_relation_labels() {
    let input = read_local_semantic_fixture("er/wide_attributes_and_summary_labels.mmd");
    let options = AsciiRenderOptions::ascii();

    let rendered = render_er_with_grid_limit(&input, &options, 10_000)
        .expect("ER diagram with wide attributes and relation labels should render");

    for expected in [
        "CUSTOMER",
        "名称",
        "ORDER",
        "状态🚀",
        "AUDIT",
        "下单🚀",
        "记录数据",
    ] {
        assert!(
            rendered.contains(expected),
            "wide ER fixture should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        rendered
            .lines()
            .all(|line| !line.contains("<br>") || line.contains("authored(bytes=")),
        "wide ER relations should expose Mermaid break syntax only inside authored framing:\n{rendered}"
    );
}