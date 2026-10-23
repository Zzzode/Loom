fn gantt_repeated_section_names_preserve_authored_task_occurrences() {
    let rendered = render_parsed(concat!(
        "gantt\n",
        "dateFormat YYYY-MM-DD\n",
        "section A\n",
        "First: first, 2026-01-01, 1d\n",
        "section B\n",
        "Middle: middle, 2026-01-02, 1d\n",
        "section A\n",
        "Last: last, 2026-01-03, 1d\n",
    ));

    let structure = rendered
        .lines()
        .filter(|line| line.starts_with("section(") || line.starts_with("  - task("))
        .collect::<Vec<_>>();
    assert_eq!(
        structure.len(),
        6,
        "unexpected Gantt structure:\n{rendered}"
    );
    assert_eq!(structure[0], "section(bytes=1)=\"A\"");
    assert!(structure[1].contains("task(bytes=5)=\"First\""));
    assert_eq!(structure[2], "section(bytes=1)=\"B\"");
    assert!(structure[3].contains("task(bytes=6)=\"Middle\""));
    assert_eq!(structure[4], "section(bytes=1)=\"A\"");
    assert!(structure[5].contains("task(bytes=4)=\"Last\""));
}