fn flowchart_parser_asymmetric_and_paper_tape_shape_families_follow_upstream_aliases() {
    let odd = render_flowchart(
        "flowchart LR\nA@{ shape: odd, label: \"Odd\" }",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();
    assert!(odd.contains("Odd"));
    assert!(
        odd.lines().next().is_some_and(|line| line.starts_with('>')),
        "odd shape should keep its left-pointing corners:\n{odd}"
    );

    let paper_tape_aliases = ["flag", "paper-tape"].map(|shape| {
        render_flowchart(
            &format!("flowchart LR\nA@{{ shape: {shape}, label: \"Tape\" }}"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap()
    });
    let flag = &paper_tape_aliases[0];
    let paper_tape = &paper_tape_aliases[1];
    assert_eq!(flag, paper_tape);
    assert!(paper_tape.contains("Tape"));
    assert!(
        paper_tape
            .lines()
            .next()
            .is_some_and(|line| line.contains('~'))
            && paper_tape
                .lines()
                .last()
                .is_some_and(|line| line.contains('~')),
        "paper-tape aliases should keep wavy top and bottom edges:\n{paper_tape}"
    );
}