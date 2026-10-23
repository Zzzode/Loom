fn graph_and_relation_edge_labels_normalize_before_empty_checks() {
    let authored = "\tendpoint\r";
    let cases = [
        ("flowchart", "flowchart LR\nA -->|label| B"),
        ("state", "stateDiagram-v2\nA --> B: label"),
        ("class", "classDiagram\nA \"1\" --> \"many\" B : label"),
    ];

    for (family, source) in cases {
        let mut model = parse_model(source);
        match &mut model {
            RenderSemanticModel::Flowchart(model) => {
                model.edges.first_mut().expect("flowchart edge").label = Some(authored.to_string());
            }
            RenderSemanticModel::State(model) => {
                model.edges.first_mut().expect("state edge").label = authored.to_string();
            }
            RenderSemanticModel::Class(model) => {
                model
                    .relations
                    .first_mut()
                    .expect("class relation")
                    .relation_title_1 = Some(authored.to_string());
            }
            other => panic!("{family} fixture parsed as unexpected model {other:?}"),
        }

        let rendered = render_model(&model, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|error| panic!("{family} failed: {error}"));
        assert!(
            rendered.contains(r"\u{9}endpoint\u{D}"),
            "{family} normalized after trimming authored controls:\n{rendered}"
        );
    }
}