fn timeline_duplicate_section_fixture_preserves_occurrence_ownership() {
    let rendered = render_source(include_str!(
        "../../../fixtures/timeline/timeline_stress_section_name_repeated.mmd"
    ));

    assert_eq!(
        rendered,
        concat!(
            "title(bytes=22)=\"Repeated section names\"\n",
            "direction: LR\n",
            "section(bytes=8)=\"Repeated\"\n",
            "  - task(bytes=5)=\"2000 \"\n",
            "    * event(bytes=25)=\"First in repeated section\"\n",
            "section(bytes=8)=\"Repeated\"\n",
            "  - task(bytes=5)=\"2001 \"\n",
            "    * event(bytes=29)=\"Second section with same name\"\n",
            "  - task(bytes=5)=\"2002 \"\n",
            "    * event(bytes=10)=\"Third task\"",
        )
    );
}