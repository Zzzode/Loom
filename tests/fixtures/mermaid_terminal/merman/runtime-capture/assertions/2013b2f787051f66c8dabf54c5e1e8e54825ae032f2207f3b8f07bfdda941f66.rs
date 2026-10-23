fn typed_lollipop_interface_resolves_qualified_target_alias_before_owner_lookup() {
        let mut model = parsed_class_model("classDiagram\nnamespace Domain {\n  class Service\n}");
        model
            .namespace_facade_aliases
            .insert("Domain.Service".to_string(), "Service".to_string());
        model.interfaces.push(ClassInterface {
            id: "interface0".to_string(),
            label: "IService".to_string(),
            class_id: "Domain.Service".to_string(),
        });
        let policy = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);
        let mut resources = ResourceContext::new(policy);

        let endpoint_index = ClassEndpointIndex::new(&model, &mut resources)
            .expect("qualified typed lollipop target should resolve through its facade alias");
        let endpoint = endpoint_index
            .resolve("interface0")
            .expect("typed lollipop interface should be indexed");

        assert_eq!(endpoint.resolved_id, "interface0");
        assert_eq!(endpoint.owner, Some("Domain"));
    }