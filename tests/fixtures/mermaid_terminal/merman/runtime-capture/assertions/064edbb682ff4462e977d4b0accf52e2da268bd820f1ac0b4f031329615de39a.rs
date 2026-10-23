fn class_summary_frames_swapped_control_and_literal_escape_endpoint_ids_injectively() {
        let summary = |first_id: &str, second_id: &str| {
            let mut model = class_summary_model();
            let mut first = model
                .classes
                .shift_remove("Source<&中")
                .expect("source should exist");
            first.id = first_id.to_string();
            first.dom_id.clone_from(&first.id);
            first.text.clone_from(&first.id);
            let mut second = model
                .classes
                .shift_remove("Target<&中")
                .expect("target should exist");
            second.id = second_id.to_string();
            second.dom_id.clone_from(&second.id);
            second.text.clone_from(&second.id);
            model.classes.insert(first.id.clone(), first);
            model.classes.insert(second.id.clone(), second);
            for relation in &mut model.relations {
                for endpoint in [&mut relation.id1, &mut relation.id2] {
                    if endpoint.as_str() == "Source<&中" {
                        *endpoint = first_id.to_string();
                    } else if endpoint.as_str() == "Target<&中" {
                        *endpoint = second_id.to_string();
                    }
                }
            }
            model.relations[0].title = "self".to_string();
            model.relations[1].title = "ab".to_string();

            let rendered = render_class_summary_fixture(
                &model,
                &AsciiRenderOptions::ascii(),
                unbounded_policy(),
                &Cell::new(false),
            )
            .0
            .expect("class summary identity fixture should render");
            rendered
                .split_once("relations:\n")
                .expect("bidirectional relations should use summary fallback")
                .1
                .to_string()
        };

        let control_first = summary("\u{1b}", r"\u{1B}");
        let literal_escape_first = summary(r"\u{1B}", "\u{1b}");
        let control_first_ab = control_first
            .lines()
            .find(|line| line.ends_with(": ab"))
            .expect("control-first summary should retain the ab relation");
        let literal_escape_first_ab = literal_escape_first
            .lines()
            .find(|line| line.ends_with(": ab"))
            .expect("literal-first summary should retain the ab relation");

        assert!(
            control_first_ab.starts_with(r#"id(bytes=1)="\u{1B}""#)
                && control_first_ab.contains(r#"id(bytes=6)="\\u{1B}""#),
            "control-first endpoint identity must remain injective:\n{control_first}"
        );
        assert!(
            literal_escape_first_ab.starts_with(r#"id(bytes=6)="\\u{1B}""#)
                && literal_escape_first_ab.contains(r#"id(bytes=1)="\u{1B}""#),
            "literal-first endpoint identity must remain injective:\n{literal_escape_first}"
        );
        assert_ne!(control_first, literal_escape_first);
    }