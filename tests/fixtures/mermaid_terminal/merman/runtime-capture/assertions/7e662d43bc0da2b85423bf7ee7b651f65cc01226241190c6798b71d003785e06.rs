fn kanban_parser_projection_keeps_group_metadata() {
    let rendered =
        render_parsed("kanban\n  root@{ priority: high, assigned: alice, icon: star }\n");

    assert_eq!(
        rendered,
        concat!(
            "group(bytes=4)=\"root\" [id(bytes=4)=\"root\", priority(bytes=4)=\"high\",\n",
            "assigned(bytes=5)=\"alice\", icon(bytes=4)=\"star\"]",
        )
    );
}