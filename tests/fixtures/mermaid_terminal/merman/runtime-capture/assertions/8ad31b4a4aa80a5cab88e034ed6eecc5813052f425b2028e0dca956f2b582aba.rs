fn projection_resolution_is_capability_owned() {
        let timeline = merman_core::diagram::RenderSemanticModel::Timeline(Default::default());
        assert_eq!(
            projection_for(capability_for(&timeline)),
            AsciiProjection::StructuredText
        );

        let parsed = merman_core::Engine::new()
            .parse_diagram_for_render_model_sync(
                "gitGraph\ncommit id:\"root\"",
                merman_core::ParseOptions::strict(),
            )
            .expect("GitGraph should parse")
            .expect("GitGraph should be detected");
        let (_, git_graph) = parsed.into_parts();
        let capability = capability_for(&git_graph).expect("GitGraph capability should resolve");
        assert_eq!(capability.diagram_type, "gitgraph");
        assert_eq!(
            projection_for(Some(capability)),
            AsciiProjection::StructuredText
        );
        assert!(capability.structured_text_fallback);
    }