fn sequence_option_mirrors_participant_boxes_below_lifelines() {
    let options = AsciiRenderOptions::unicode().with_sequence_mirror_actors(true);

    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "    participant U as User\n",
            "    participant B as Browser\n",
            "    participant S as Server\n",
            "    U->>B: Click Login\n",
            "    B->>S: POST /login\n",
            "    S-->>B: Return Token\n",
            "    B-->>U: Show Success\n",
        ),
        &options,
    )
    .unwrap();

    let rows = rendered.lines().collect::<Vec<_>>();
    let footer = &rows[rows.len() - 3..];
    let cap = rows[2]
        .chars()
        .map(|ch| match ch {
            '└' => '┌',
            '┘' => '┐',
            '┬' => '┴',
            other => other,
        })
        .collect::<String>();
    assert_eq!(
        footer[0], cap,
        "mirrored boxes must join the same actor columns:\n{rendered}"
    );
    assert_eq!(
        footer[1], rows[1],
        "mirrored labels must retain header geometry:\n{rendered}"
    );
    let bottom = rows[0]
        .chars()
        .map(|ch| match ch {
            '┌' => '└',
            '┐' => '┘',
            other => other,
        })
        .collect::<String>();
    assert_eq!(
        footer[2], bottom,
        "mirrored boxes must close below the lifelines:\n{rendered}"
    );
    let lifeline_row = rows[rows.len() - 4].chars().collect::<Vec<_>>();
    for (column, glyph) in rows[2].chars().enumerate() {
        if glyph == '┬' {
            assert_eq!(
                lifeline_row.get(column),
                Some(&'│'),
                "every mirrored participant must connect to its lifeline:\n{rendered}"
            );
        }
    }
}