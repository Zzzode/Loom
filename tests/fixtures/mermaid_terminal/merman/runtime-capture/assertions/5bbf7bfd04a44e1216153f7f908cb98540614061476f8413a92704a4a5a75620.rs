fn class_interface_targets_reject_interface_ids_independent_of_declaration_order() {
        for reversed in [false, true] {
            let mut model = parsed_class_model("classDiagram\nclass Target");
            model.interfaces = vec![
                ClassInterface {
                    id: "interface0".to_string(),
                    label: "IBase".to_string(),
                    class_id: "Target".to_string(),
                },
                ClassInterface {
                    id: "interface1".to_string(),
                    label: "IForged".to_string(),
                    class_id: "interface0".to_string(),
                },
            ];
            if reversed {
                model.interfaces.reverse();
            }

            let policy = unbounded_policy();
            let error = render_class_diagram_with_execution(
                &model,
                &AsciiRenderOptions::ascii(),
                AsciiExecution::for_test(&policy),
            )
            .expect_err("interface targets must resolve to authored classes");

            assert_eq!(
                error,
                AsciiError::UnsupportedFeature {
                    diagram_type: "class",
                    feature: "interfaces with missing target classes",
                },
                "interface declaration order must not change validation"
            );
        }
    }