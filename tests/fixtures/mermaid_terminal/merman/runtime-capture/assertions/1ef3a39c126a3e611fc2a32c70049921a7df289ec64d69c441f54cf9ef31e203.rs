fn sequence_extended_actor_types_render_as_participant_boxes() {
    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "participant API@{ \"type\" : \"boundary\", \"alias\": \"Public API\" }\n",
            "participant Auth@{ \"type\" : \"control\" } as Auth Controller\n",
            "participant Entity@{ \"type\" : \"entity\" }\n",
            "participant DB@{ \"type\" : \"database\" }\n",
            "participant Queue@{ \"type\" : \"queue\" }\n",
            "actor Store@{ \"type\" : \"collections\" } as Event Store\n",
            "API->>Auth: Request\n",
            "Auth->>Entity: Validate\n",
            "Entity->>DB: Query\n",
            "DB-->>Queue: Result\n",
            "Queue-->>Store: Publish",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("extended actor types should render as ASCII participant boxes");

    for label in [
        "Public API",
        "Auth Controller",
        "Entity",
        "DB",
        "Queue",
        "Event Store",
    ] {
        assert!(
            rendered.contains(label),
            "extended actor label {label:?} should render:\n{rendered}"
        );
    }
    for message in ["Request", "Validate", "Query", "Result", "Publish"] {
        assert!(
            rendered.contains(message),
            "messages involving extended actor types should render:\n{rendered}"
        );
    }
}