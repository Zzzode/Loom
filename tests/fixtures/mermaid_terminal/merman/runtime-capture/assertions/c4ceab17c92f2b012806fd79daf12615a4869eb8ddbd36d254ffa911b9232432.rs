fn namespace_scope_index_rejects_cyclic_parent_chains_before_render_descent() {
        let mut model = parsed_class_model(
            "classDiagram\nnamespace Left {\n  class A\n}\nnamespace Right {\n  class B\n}\nA --> B",
        );
        model
            .namespaces
            .get_mut("Left")
            .expect("Left namespace should exist")
            .parent = Some("Right".to_string());
        model
            .namespaces
            .get_mut("Right")
            .expect("Right namespace should exist")
            .parent = Some("Left".to_string());
        let policy = AsciiResourcePolicy::for_profile(
            merman_core::resources::ResourceProfile::UnboundedForTrustedInput,
        );
        let mut endpoint_resources = ResourceContext::new(policy);
        let endpoint_index = ClassEndpointIndex::new(&model, &mut endpoint_resources)
            .expect("class endpoint index should build");
        let resources = ResourceContext::new(policy);

        let error = NamespaceScopeIndex::new(
            &model,
            &endpoint_index,
            &resources,
            AsciiExecution::for_test(&policy),
        )
        .expect_err("cyclic namespace parents must reject while building the scope index");
        assert!(matches!(
            error,
            AsciiError::UnsupportedFeature {
                diagram_type: "class",
                feature: "inconsistent class namespace ownership",
            }
        ));
        assert_eq!(resources.layout_work_used(), 0);
        assert_eq!(resources.document_cells_used(), 0);
    }