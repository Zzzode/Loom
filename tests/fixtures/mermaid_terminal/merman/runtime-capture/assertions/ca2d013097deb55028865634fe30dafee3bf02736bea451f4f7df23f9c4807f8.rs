fn journey_duplicate_sections_preserve_parser_occurrence_ownership() {
    let rendered = render_source(concat!(
        "journey\n",
        "title Repeated journey\n",
        "section Repeated\n",
        "  First: 5: Alice\n",
        "section Repeated\n",
        "  Second: 3: Bob\n",
    ));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=16)=\"Repeated journey\"\n",
            "actors=[bytes=5 \"Alice\", bytes=3 \"Bob\"]\n",
            "section(bytes=8)=\"Repeated\"\n",
            "  - task(bytes=7)=\"  First\" score=5 people=[bytes=5 \"Alice\"]\n",
            "section(bytes=8)=\"Repeated\"\n",
            "  - task(bytes=8)=\"  Second\" score=3 people=[bytes=3 \"Bob\"]",
        )
    );
}