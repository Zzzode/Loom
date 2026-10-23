fn mindmap_parser_preserves_authored_ids_in_structured_text() {
    let rendered = render_parsed("mindmap\nroot[The root]\n  theId(child1)\n    leaf1\n  child2\n");

    for authored_id in ["root", "theId", "leaf1", "child2"] {
        assert!(
            rendered.contains(&format!(
                "id(bytes={})=\"{authored_id}\"",
                authored_id.len()
            )),
            "missing authored id {authored_id}:\n{rendered}"
        );
    }
    for internal_id in ["0", "1", "2", "3"] {
        assert!(
            !rendered.contains(&format!("id(bytes=1)=\"{internal_id}\"")),
            "internal id {internal_id} leaked:\n{rendered}"
        );
    }
}