fn unsupported_candidates_match_capability_catalog_and_dispatch() {
    let workspace_root = workspace_root();
    let engine = Engine::new();
    let supported = ascii_supported_diagram_types();
    let diagrammatic = ascii_diagrammatic_diagram_types();

    for candidate in REJECTED_CANDIDATES {
        let capability = ascii_capabilities()
            .iter()
            .find(|capability| capability.diagram_type == candidate.capability_type)
            .unwrap_or_else(|| panic!("missing capability for {}", candidate.capability_type));
        assert_eq!(capability.semantic_coverage, None);
        assert_eq!(capability.primary_projection, AsciiPrimaryProjection::None);
        assert_eq!(capability.support_level, AsciiSupportLevel::Unsupported);
        assert!(!supported.contains(&candidate.capability_type));
        assert!(!diagrammatic.contains(&candidate.capability_type));

        let source = read_fixture(&workspace_root, candidate.fixture);
        let parsed = engine
            .parse_diagram_for_render_model_sync(&source, ParseOptions::strict())
            .unwrap_or_else(|error| panic!("failed to parse {}: {error}", candidate.fixture))
            .unwrap_or_else(|| panic!("failed to detect {}", candidate.fixture));
        assert_eq!(parsed.model().kind(), candidate.render_kind);

        let error = render_model(parsed.model(), &AsciiRenderOptions::ascii())
            .expect_err("rejected candidates must remain unsupported without admission");
        assert_eq!(
            error,
            AsciiError::UnsupportedDiagram {
                diagram_type: candidate.render_kind.to_string(),
            }
        );
    }
}