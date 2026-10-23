fn gantt_structured_text_discloses_dependency_source_expressions() {
    let rendered = render_parsed(concat!(
        "gantt\n",
        "dateFormat YYYY-MM-DD\n",
        "section Build\n",
        "Design: design, 2026-01-01, 1d\n",
        "Review: review, 2026-01-02, 1d\n",
        "Implement: implementation, after design review, until design review\n",
        "Release: release, 2026-01-05, 2026-01-06\n",
    ));

    assert!(
        rendered.contains("after=[bytes=6 \"design\", bytes=6 \"review\"]"),
        "structured Gantt output should disclose the dependency source expression:\n{rendered}"
    );
    assert!(rendered.contains("until=[bytes=6 \"design\", bytes=6 \"review\"]"));
    assert!(rendered.contains("start(bytes=10)=\"2026-01-01\""));
    assert!(rendered.contains("duration(bytes=2)=\"1d\""));
    assert!(rendered.contains("end(bytes=10)=\"2026-01-06\""));
    assert!(rendered.contains("id(bytes=6)=\"design\""));
    assert!(rendered.contains("id(bytes=6)=\"review\""));
    assert!(rendered.contains("id(bytes=14)=\"implementation\""));
    assert!(rendered.contains("id(bytes=7)=\"release\""));
}