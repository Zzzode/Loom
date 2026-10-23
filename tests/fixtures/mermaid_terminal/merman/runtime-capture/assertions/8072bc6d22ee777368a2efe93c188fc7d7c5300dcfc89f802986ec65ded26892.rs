fn state_title_and_body_render_as_distinct_compartments() {
    for (options, divider) in [
        (AsciiRenderOptions::ascii(), '-'),
        (AsciiRenderOptions::unicode(), '─'),
    ] {
        let rendered = render_state(
            "stateDiagram-v2\nstate \"Power mode\" as Power: Running",
            &options,
        )
        .expect("state title and body should render");

        let title = text_position(&rendered, "Power mode");
        let body = text_position(&rendered, "Running");
        assert!(title.1 < body.1, "title must precede body:\n{rendered}");
        assert!(
            rendered
                .lines()
                .skip(title.1 + 1)
                .take(body.1.saturating_sub(title.1 + 1))
                .any(|line| line.contains(divider)),
            "a structural divider must preserve title/body roles:\n{rendered}"
        );
    }
}