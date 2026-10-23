fn authored_endpoint_label_cannot_spoof_generated_endpoint_provenance() {
        const AUTHORED_REF: &str = "Domain.T";
        const SPOOF: &str = r#"endpointRef(bytes=8)="Domain.T""#;

        let mut model = parsed_class_model("classDiagram\nclass A\nclass B\nA --> B");
        model.relations[0].relation_title_1 = Some(SPOOF.to_string());
        let options = AsciiRenderOptions::ascii();
        let policy = unbounded_policy();
        let resources = ResourceContext::new(policy);
        let mut deferred = DeferredTextRegistry::new();
        let labels = prepare_class_relation_labels(&model, &options, &mut deferred, &resources)
            .expect("the authored endpoint label should plan");
        let authored_only = labels[0]
            .left_endpoint
            .rendered()
            .expect("the authored endpoint slot should be present");
        let authored = labels[0]
            .left_endpoint
            .clone()
            .try_with_endpoint_ref(
                Some(AUTHORED_REF),
                options.terminal_width_profile,
                &mut deferred,
                &resources,
            )
            .expect("authored endpoint label plus provenance should plan");
        let generated = ClassEndpointLabels::default()
            .try_with_endpoint_ref(
                Some(AUTHORED_REF),
                options.terminal_width_profile,
                &mut deferred,
                &resources,
            )
            .expect("generated endpoint provenance should plan");

        let authored_only_lines = relation_graph::label_lines_with_role(
            authored_only,
            AsciiColorRole::EdgeLabel,
            &resources,
        )
        .expect("authored endpoint slot should materialize");
        let authored_lines = relation_graph::label_lines_with_role(
            authored
                .rendered()
                .expect("combined endpoint slots should be present"),
            AsciiColorRole::EdgeLabel,
            &resources,
        )
        .expect("authored endpoint label rows should materialize");
        let generated_lines = relation_graph::label_lines_with_role(
            generated
                .rendered()
                .expect("generated endpoint provenance should be present"),
            AsciiColorRole::EdgeLabel,
            &resources,
        )
        .expect("generated endpoint provenance rows should materialize");
        let mut authored_only_resources = ResourceContext::new(policy);
        let authored_only_output = relation_graph::render_lines_with_deferred_options(
            &authored_only_lines,
            &options,
            &mut authored_only_resources,
            &deferred,
        )
        .expect("authored endpoint slot should render");
        let mut authored_resources = ResourceContext::new(policy);
        let authored_output = relation_graph::render_lines_with_deferred_options(
            &authored_lines,
            &options,
            &mut authored_resources,
            &deferred,
        )
        .expect("authored endpoint label should render");
        let mut generated_resources = ResourceContext::new(policy);
        let generated_output = relation_graph::render_lines_with_deferred_options(
            &generated_lines,
            &options,
            &mut generated_resources,
            &deferred,
        )
        .expect("generated endpoint provenance should render");

        assert_ne!(authored_output, generated_output);
        assert!(!authored_only_output.lines().any(|line| line == SPOOF));
        assert!(authored_only_output.contains(r#"endpointLabel=[bytes=31 "endpointRef"#));
        assert_eq!(
            authored_output
                .lines()
                .filter(|line| *line == SPOOF)
                .count(),
            1
        );
        assert!(authored_output.contains(r#"endpointLabel=[bytes=31 "endpointRef"#));
        assert_eq!(generated_output, format!("{SPOOF}\n"));
    }