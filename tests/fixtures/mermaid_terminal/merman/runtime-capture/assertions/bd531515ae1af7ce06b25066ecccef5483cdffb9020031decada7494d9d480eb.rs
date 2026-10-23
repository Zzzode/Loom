fn reconstructed_generic_member_admits_layout_work_before_materializing() {
        let mut model =
            parsed_class_model("classDiagram\nclass Service {\n  #compute(input) Result\n}");
        let member = model
            .classes
            .get_mut("Service")
            .and_then(|class| class.methods.first_mut())
            .expect("Service method should exist");
        member.visibility = "#".to_string();
        member.id = "compute~Array~Array~T~~~".to_string();
        member.parameters = "input: Map~Key,Value~".to_string();
        member.return_type = "Result~List~T~~".to_string();
        member.classifier = "*".to_string();
        member.display_text.clear();

        let member = model
            .classes
            .get("Service")
            .and_then(|class| class.methods.first())
            .expect("Service method should remain available");
        let unbounded = AsciiResourcePolicy::default();
        let measured = ResourceContext::new(unbounded);
        let materialized = Cell::new(false);
        let rendered = member_text_with_probe(member, &measured, || materialized.set(true))
            .expect("unbounded generic member should render");
        let exact_work = measured.layout_work_used();
        assert!(exact_work > 1);
        assert!(materialized.get());
        assert!(
            rendered.contains("compute<Array<Array<T>>>") && rendered.contains("Map<Key,Value>")
        );

        let exact_policy = unbounded
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, exact_work)
            .expect("exact generic-member work limit should be valid");
        let exact_resources = ResourceContext::new(exact_policy);
        let exact_materialized = Cell::new(false);
        member_text_with_probe(member, &exact_resources, || exact_materialized.set(true))
            .expect("exact generic-member work should permit materialization");
        assert!(exact_materialized.get());
        assert_eq!(exact_resources.layout_work_used(), exact_work);

        let below_policy = unbounded
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, exact_work - 1)
            .expect("max-minus-one generic-member work limit should be valid");
        let below_resources = ResourceContext::new(below_policy);
        let below_materialized = Cell::new(false);
        let error =
            member_text_with_probe(member, &below_resources, || below_materialized.set(true))
                .expect_err("max-minus-one generic-member work must reject before materialization");
        assert!(!below_materialized.get());
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
                    && details.actual > details.max
        ));
    }