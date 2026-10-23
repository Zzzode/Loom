fn state_dividers_render_as_stacked_sections() {
    let rendered = render_state(
        "stateDiagram-v2\nstate Active {\n  A\n  --\n  B\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("state dividers should render as stacked sections");

    assert!(
        rendered.contains("Active"),
        "parent composite state should render:\n{rendered}"
    );
    assert!(
        rendered.contains("A") && rendered.contains("B"),
        "divider sections should keep their child states visible:\n{rendered}"
    );
    assert!(
        rendered.lines().filter(|line| line.contains("...")).count() >= 2,
        "divider sections should render horizontal separators:\n{rendered}"
    );
    assert!(
        !rendered.contains("divider-id") && !rendered.contains("id-"),
        "divider implementation ids should not leak into ASCII output:\n{rendered}"
    );
}