fn namespace_scope_index_admits_exact_work_and_rolls_back_n_minus_one() {
        let model = parsed_class_model(
            "classDiagram\nnamespace Platform {\n  namespace FFI {\n    class Dart\n  }\n  namespace Core {\n    class Renderer\n  }\n}\nDart --> Renderer",
        );
        let unbounded = AsciiResourcePolicy::for_profile(
            merman_core::resources::ResourceProfile::UnboundedForTrustedInput,
        );
        let mut endpoint_resources = ResourceContext::new(unbounded);
        let endpoint_index = ClassEndpointIndex::new(&model, &mut endpoint_resources)
            .expect("class endpoint index should build");
        let measured = ResourceContext::new(unbounded);
        NamespaceScopeIndex::new(
            &model,
            &endpoint_index,
            &measured,
            AsciiExecution::for_test(&unbounded),
        )
        .expect("unbounded namespace index should build");
        let exact_work = measured.layout_work_used();
        assert!(exact_work > 1);

        let exact_policy = unbounded
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, exact_work)
            .expect("exact namespace-index work limit should be valid");
        let exact = ResourceContext::new(exact_policy);
        NamespaceScopeIndex::new(
            &model,
            &endpoint_index,
            &exact,
            AsciiExecution::for_test(&exact_policy),
        )
        .expect("exact namespace-index work should build");
        assert_eq!(exact.layout_work_used(), exact_work);

        let below_policy = unbounded
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, exact_work)
            .expect("max-minus-one namespace-index work limit should be valid");
        let below = ResourceContext::new(below_policy);
        below
            .charge_layout_work(1)
            .expect("test checkpoint should be admitted");
        let checkpoint = below.layout_work_used();
        let error = NamespaceScopeIndex::new(
            &model,
            &endpoint_index,
            &below,
            AsciiExecution::for_test(&below_policy),
        )
        .expect_err("max-minus-one namespace-index work must reject");
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
                    && details.actual > details.max
        ));
        assert_eq!(below.layout_work_used(), checkpoint);
        assert_eq!(below.document_cells_used(), 0);
    }