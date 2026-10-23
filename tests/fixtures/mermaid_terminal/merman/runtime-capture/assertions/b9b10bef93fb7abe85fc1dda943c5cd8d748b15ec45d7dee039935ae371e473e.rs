fn er_summary_frames_direct_authored_identities_that_share_one_display_label() {
    let control_identity = "\u{1b}".to_string();
    let authored_escape_identity = r"\u{1B}".to_string();
    let control_rendered_id = "entity-control-0".to_string();
    let escape_rendered_id = "entity-escape-1".to_string();
    let bridge_rendered_id = "entity-bridge-2".to_string();
    let mut model = ErDiagramRenderModel::default();
    model.entities.insert(
        control_identity.clone(),
        ErEntityRenderModel {
            id: control_rendered_id.clone(),
            label: "Same".to_string(),
            ..ErEntityRenderModel::default()
        },
    );
    model.entities.insert(
        authored_escape_identity.clone(),
        ErEntityRenderModel {
            id: escape_rendered_id.clone(),
            label: "Same".to_string(),
            ..ErEntityRenderModel::default()
        },
    );
    model.entities.insert(
        "bridge".to_string(),
        ErEntityRenderModel {
            id: bridge_rendered_id.clone(),
            label: "Bridge".to_string(),
            ..ErEntityRenderModel::default()
        },
    );
    model.relationships = vec![
        ErRelationshipRenderModel {
            entity_a: control_rendered_id.clone(),
            role_a: "first".to_string(),
            entity_b: bridge_rendered_id.clone(),
            rel_spec: ErRelSpecRenderModel {
                card_a: "ONLY_ONE".to_string(),
                card_b: "ONLY_ONE".to_string(),
                rel_type: "IDENTIFYING".to_string(),
            },
        },
        ErRelationshipRenderModel {
            entity_a: bridge_rendered_id.clone(),
            role_a: "second".to_string(),
            entity_b: escape_rendered_id.clone(),
            rel_spec: ErRelSpecRenderModel {
                card_a: "ONLY_ONE".to_string(),
                card_b: "ONLY_ONE".to_string(),
                rel_type: "IDENTIFYING".to_string(),
            },
        },
        ErRelationshipRenderModel {
            entity_a: control_rendered_id.clone(),
            role_a: "spanning".to_string(),
            entity_b: escape_rendered_id.clone(),
            rel_spec: ErRelSpecRenderModel {
                card_a: "ONLY_ONE".to_string(),
                card_b: "ONLY_ONE".to_string(),
                rel_type: "IDENTIFYING".to_string(),
            },
        },
    ];

    let rendered = render_er_model(&model, &AsciiRenderOptions::ascii())
        .expect("direct ER identities should remain recoverable");

    assert!(rendered.contains("relations:"), "{rendered}");
    assert_eq!(
        rendered
            .lines()
            .filter(|line| line.contains("| Same"))
            .count(),
        2,
        "{rendered}"
    );
    assert!(
        rendered.contains(r#"id(bytes=1)="\u{1B}""#),
        "the normalized control id must retain its authored byte identity:\n{rendered}"
    );
    assert!(
        rendered.contains(r#"id(bytes=6)="\\u{1B}""#),
        "the authored escape must remain distinct from the raw control id:\n{rendered}"
    );
    for rendered_id in [control_rendered_id, escape_rendered_id, bridge_rendered_id] {
        assert!(
            !rendered.contains(&rendered_id),
            "renderer-owned entity ids must not leak into summaries:\n{rendered}"
        );
    }
}