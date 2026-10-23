fn resolved_actor_lifecycles_require_actor_order_alignment() {
        let mut model = shared_anchor_model();
        model
            .actor_lifecycles
            .as_mut()
            .expect("parser-backed model should expose resolved lifecycle truth")
            .pop();
        let participant_index = participant_index(&model);
        let policy = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);
        let mut resources = ResourceContext::new(policy);

        let error = resolve_actor_lifecycles(
            &model,
            &participant_index,
            &mut resources,
            AsciiExecution::for_test(&policy),
        )
        .expect_err("misaligned lifecycle sidecars must be rejected");
        assert!(matches!(
            error,
            AsciiError::UnsupportedFeature {
                diagram_type: "sequence",
                feature: "actor lifecycle actors",
            }
        ));
    }