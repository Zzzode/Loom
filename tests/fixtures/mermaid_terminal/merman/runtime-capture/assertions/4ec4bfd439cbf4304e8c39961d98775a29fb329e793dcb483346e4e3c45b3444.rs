fn gantt_direct_model_renders_only_typed_constraints() {
    let mut model = GanttDiagramRenderModel::default();
    model.sections = vec!["Build".to_string()];
    model.tasks = vec![GanttRenderTask {
        id: "implementation".to_string(),
        task: "Implement".to_string(),
        section: "Build".to_string(),
        start_constraint: GanttTaskStartConstraint::After {
            dependency_ids: vec!["design".to_string(), "review".to_string()],
        },
        end_constraint: GanttTaskEndConstraint::Until {
            dependency_ids: vec!["release".to_string()],
        },
        prev_task_id: Some("legacy-previous".to_string()),
        raw: GanttRenderTaskRaw {
            data: "implementation,after raw-start,until raw-end".to_string(),
            start_time: GanttRenderTaskStart::GetStartDate {
                start_data: "after raw-start".to_string(),
            },
            end_time: GanttRenderTaskEnd {
                data: "until raw-end".to_string(),
            },
        },
        ..Default::default()
    }];

    let rendered = render(RenderSemanticModel::Gantt(model));
    assert!(rendered.contains("after=[bytes=6 \"design\", bytes=6 \"review\"]"));
    assert!(rendered.contains("until=[bytes=7 \"release\"]"));
    for legacy in ["raw-start", "raw-end", "legacy-previous"] {
        assert!(
            !rendered.contains(legacy),
            "ASCII must not recover constraints from legacy raw fields:\n{rendered}"
        );
    }
}