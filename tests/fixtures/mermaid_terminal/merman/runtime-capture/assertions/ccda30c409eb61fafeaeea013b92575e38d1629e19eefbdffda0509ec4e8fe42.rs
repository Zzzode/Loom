fn class_local_semantic_fixture_covers_standalone_note() {
    let input = read_local_semantic_fixture("class/standalone_note.mmd");
    let rendered =
        render_class(&input, &AsciiRenderOptions::ascii()).expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+------------+\n",
            "| note       |\n",
            "| Standalone |\n",
            "+------------+\n",
        )
    );
}