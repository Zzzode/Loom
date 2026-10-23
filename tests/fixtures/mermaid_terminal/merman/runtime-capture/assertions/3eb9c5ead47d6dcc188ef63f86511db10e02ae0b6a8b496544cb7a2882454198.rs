fn sequence_spaced_actor_names_render_without_splitting_participants() {
    let input = r#"sequenceDiagram
participant cron job
participant data svc
cron job ()->>() data svc: run
Note over cron job,data svc: nightly
data svc-->>cron job: done"#;

    for options in [AsciiRenderOptions::ascii(), AsciiRenderOptions::unicode()] {
        let rendered = render_sequence(input, &options)
            .expect("Mermaid-valid spaced actor names should render");
        for expected in ["cron job", "data svc", "run", "nightly", "done"] {
            assert!(
                rendered.contains(expected),
                "spaced sequence should retain {expected:?}:\n{rendered}"
            );
        }
    }
}