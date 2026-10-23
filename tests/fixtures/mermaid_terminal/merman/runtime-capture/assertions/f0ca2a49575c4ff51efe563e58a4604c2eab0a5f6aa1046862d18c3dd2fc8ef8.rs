fn parser_lollipop_interface_inherits_target_namespace_for_placement_and_routing() {
        let model = parsed_class_model(concat!(
            "classDiagram\n",
            "namespace Domain {\n  class Service\n}\n",
            "IService ()-- Domain.Service",
        ));
        let policy = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);
        let mut resources = ResourceContext::new(policy);
        validate_class_namespace_ownership(&model, &mut resources)
            .expect("parser namespace ownership should be internally consistent");
        let endpoint_index = ClassEndpointIndex::new(&model, &mut resources)
            .expect("class endpoint index should inherit interface ownership");

        let interface = model
            .interfaces
            .first()
            .expect("lollipop relation should create an interface endpoint");
        assert_eq!(interface.class_id, "Domain.Service");
        assert_eq!(
            model
                .namespace_facade_aliases
                .get("Domain.Service")
                .map(String::as_str),
            Some("Service")
        );
        let endpoint = endpoint_index
            .resolve(interface.id.as_str())
            .expect("interface endpoint should be indexed");
        assert_eq!(endpoint.owner, Some("Domain"));

        let interface_index = NamespaceInterfaceIndex::new(
            &model,
            &endpoint_index,
            &resources,
            AsciiExecution::for_test(&policy),
        )
        .expect("namespace interface placement should build");
        assert!(interface_index.for_owner(None).is_empty());
        assert_eq!(
            interface_index
                .for_owner(Some("Domain"))
                .iter()
                .map(|interface| interface.id.as_str())
                .collect::<Vec<_>>(),
            vec!["interface0"]
        );

        let scope_index = NamespaceScopeIndex::new(
            &model,
            &endpoint_index,
            &resources,
            AsciiExecution::for_test(&policy),
        )
        .expect("interface relation scope should build");
        assert_eq!(scope_index.scope_for_relation(0), Some("Domain"));
    }