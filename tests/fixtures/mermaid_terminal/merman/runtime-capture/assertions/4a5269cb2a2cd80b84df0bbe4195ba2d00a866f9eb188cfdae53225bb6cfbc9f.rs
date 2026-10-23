fn shared_anchor_resolution_is_ordered_and_budgeted_before_materialization() {
        let model = shared_anchor_model();
        let participant_index = participant_index(&model);
        let unbounded = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);
        let mut resources = ResourceContext::new(unbounded);
        let lifecycles = resolve_actor_lifecycles(
            &model,
            &participant_index,
            &mut resources,
            AsciiExecution::for_test(&unbounded),
        )
        .expect("shared lifecycle anchors should consume successive signals");
        let total_work = resources.layout_work_used();

        assert_eq!(lifecycles[participant_index["B"]].created_at, Some(0));
        assert_eq!(lifecycles[participant_index["A"]].destroyed_at, Some(1));

        let exact = unbounded
            .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, total_work)
            .expect("exact lifecycle work limit should be valid");
        let mut exact_resources = ResourceContext::new(exact);
        resolve_actor_lifecycles(
            &model,
            &participant_index,
            &mut exact_resources,
            AsciiExecution::for_test(&exact),
        )
        .expect("exact lifecycle work limit should pass");
        assert_eq!(exact_resources.layout_work_used(), total_work);

        let below = unbounded
            .with_limit(
                AsciiResourceLimitId::MaxLayoutWorkUnits,
                total_work.saturating_sub(1),
            )
            .expect("lifecycle work limit minus one should be valid");
        let mut below_resources = ResourceContext::new(below);
        let error = resolve_actor_lifecycles(
            &model,
            &participant_index,
            &mut below_resources,
            AsciiExecution::for_test(&below),
        )
        .expect_err("lifecycle planning must reject before request materialization");
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details)
                if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
                    && details.actual == total_work
                    && details.max == total_work - 1
        ));
        assert_eq!(below_resources.layout_work_used(), 0);
    }