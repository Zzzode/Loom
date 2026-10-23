fn er_aliases_preserve_authored_entity_identities_when_display_labels_collide() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "A[\"X\"]\n",
            "B[\"X\"]\n",
            "C[\"X\"]\n",
            "A ||--|| B : first\n",
            "A ||..o{ B : second\n",
            "A ||--|| C : third\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("colliding ER aliases should keep lossless relationship identities");

    for identity in ["A", "B", "C"].map(framed_er_summary_endpoint) {
        assert!(
            rendered.contains(&identity),
            "ER boxes must preserve authored endpoint identity {identity:?}:\n{rendered}"
        );
    }
    for label in ["first", "second", "third"] {
        assert!(rendered.contains(label), "missing {label:?}:\n{rendered}");
    }
    for generated_id in ["entity-A-0", "entity-B-1", "entity-C-2"] {
        assert!(
            !rendered.contains(generated_id),
            "generated entity ids must not leak into summaries:\n{rendered}"
        );
    }
}