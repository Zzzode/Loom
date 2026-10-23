fn sequence_actor_links_do_not_block_ascii_rendering() {
    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "participant A\n",
            "participant B\n",
            "links A: { \"Docs\": \"https://example.com/docs\" }\n",
            "link B: Repo @ https://example.com/repo\n",
            "A->>B: Hi",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence actor links should not block ASCII rendering");

    assert!(
        rendered.contains("Hi"),
        "linked actors should keep sequence messages renderable:\n{rendered}"
    );
    assert!(
        !rendered.contains("example.com"),
        "actor link URLs are SVG metadata and should not leak into ASCII output:\n{rendered}"
    );
}