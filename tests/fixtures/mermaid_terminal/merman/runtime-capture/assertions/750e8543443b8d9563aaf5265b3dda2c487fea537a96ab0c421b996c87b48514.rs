fn class_relation_text_discloses_lossy_authored_projection_in_routed_and_summary_output() {
        const AUTHORED: &str = "\u{1b}";
        const VISIBLE: &str = r"\u{1B}";
        const DISCLOSURE: &str = r#"authored(bytes=1)="\u{1B}""#;

        let routed = |value: &str| {
            let mut model = parsed_class_model("classDiagram\nclass A\nclass B\nA --> B");
            model.notes.push(note("note0", value));
            let relation = model
                .relations
                .first_mut()
                .expect("routed fixture should contain a relation");
            relation.title = value.to_string();
            relation.relation_title_1 = Some(value.to_string());
            render_class_identity_fixture(&model)
        };
        let authored_routed = routed(AUTHORED);
        let visible_routed = routed(VISIBLE);
        assert_ne!(authored_routed, visible_routed);
        assert_eq!(authored_routed.matches(DISCLOSURE).count(), 3);
        assert!(!visible_routed.contains("authored(bytes="));

        let html_break_routed = routed("left<br>right");
        let mermaid_break_routed = routed(r"left\nright");
        let line_break_routed = routed("left\nright");
        assert_ne!(html_break_routed, mermaid_break_routed);
        assert_ne!(html_break_routed, line_break_routed);
        assert_ne!(mermaid_break_routed, line_break_routed);
        assert_eq!(
            html_break_routed
                .matches(r#"authored(bytes=13)="left<br>right""#)
                .count(),
            3
        );
        assert!(mermaid_break_routed.contains(r#"authored(bytes=11)="left\\nright""#));
        assert!(line_break_routed.contains(r#"authored(bytes=10)="left\nright""#));

        let trimmed_routed = routed(" label ");
        let untrimmed_routed = routed("label");
        assert_ne!(trimmed_routed, untrimmed_routed);
        assert_eq!(
            trimmed_routed
                .matches(r#"authored(bytes=7)=" label ""#)
                .count(),
            3
        );
        assert!(!untrimmed_routed.contains("authored(bytes="));

        let spoofed_routed = routed(r#"\u{1B}<br>authored(bytes=1)="\u{1B}""#);
        assert_ne!(authored_routed, spoofed_routed);
        assert!(
            spoofed_routed.matches("authored(bytes=").count()
                > authored_routed.matches("authored(bytes=").count()
        );

        let controls_first = routed("\u{1b}\\u{7F}");
        let controls_last = routed("\\u{1B}\u{7f}");
        assert_ne!(controls_first, controls_last);

        let summary = |value: &str| {
            let mut model = class_summary_model();
            for relation in &mut model.relations {
                relation.title = value.to_string();
                relation.relation_title_1 = Some(value.to_string());
                relation.relation_title_2 = Some(value.to_string());
            }
            render_class_summary_fixture(
                &model,
                &AsciiRenderOptions::ascii(),
                unbounded_policy(),
                &Cell::new(false),
            )
            .0
            .expect("class summary identity fixture should render")
        };
        let authored_summary = summary(AUTHORED);
        let visible_summary = summary(VISIBLE);
        assert!(authored_summary.contains("relations:"));
        assert_ne!(authored_summary, visible_summary);
        assert!(authored_summary.contains(DISCLOSURE));
        assert!(!visible_summary.contains("authored(bytes="));

        let html_break_summary = summary("left<br>right");
        let mermaid_break_summary = summary(r"left\nright");
        let line_break_summary = summary("left\nright");
        assert_ne!(html_break_summary, mermaid_break_summary);
        assert_ne!(html_break_summary, line_break_summary);
        assert_ne!(mermaid_break_summary, line_break_summary);
        assert!(html_break_summary.contains(r#"authored(bytes=13)="left<br>right""#));
        assert!(mermaid_break_summary.contains(r#"authored(bytes=11)="left\\nright""#));
        assert!(line_break_summary.contains(r#"authored(bytes=10)="left\nright""#));

        let endpoint = |value: Option<&str>| {
            let mut model = parsed_class_model("classDiagram\nclass A\nclass B\nA --> B");
            let relation = model
                .relations
                .first_mut()
                .expect("endpoint identity fixture should contain a relation");
            relation.relation_title_1 = value.map(str::to_string);
            render_class_identity_fixture(&model)
        };
        let absent_endpoint = endpoint(None);
        let empty_endpoint = endpoint(Some(""));
        let blank_endpoint = endpoint(Some(" "));
        assert_ne!(absent_endpoint, empty_endpoint);
        assert_ne!(empty_endpoint, blank_endpoint);
        assert!(empty_endpoint.contains(r#"authored(bytes=0)="""#));
        assert!(blank_endpoint.contains(r#"authored(bytes=1)=" ""#));
    }