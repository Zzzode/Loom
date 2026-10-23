fn sequence_nested_loop_inside_alt_keeps_frame_padding() {
    let input = "sequenceDiagram
    participant Client
    participant API
    participant Worker
    Client->>API: Submit job
    alt Valid request
      API->>Worker: Queue work
      loop Poll status
        Client->>API: GET /jobs/123
        API-->>Client: Running
      end
    else Invalid request
      API-->>Client: 400 Bad Request
    end";

    let rendered = render_sequence(input, &AsciiRenderOptions::ascii())
        .expect("nested loop inside alt should render");

    let loop_top = rendered
        .lines()
        .find(|line| line.contains("loop Poll status"))
        .unwrap_or_else(|| panic!("loop frame should render:\n{rendered}"));
    let outer_top = rendered
        .lines()
        .find(|line| line.contains("alt Valid request"))
        .unwrap_or_else(|| panic!("outer alt frame should render:\n{rendered}"));
    let outer_left = outer_top
        .find('+')
        .unwrap_or_else(|| panic!("outer alt frame should have a left border:\n{rendered}"));
    let loop_left = loop_top
        .find('+')
        .unwrap_or_else(|| panic!("nested loop should have a left border:\n{rendered}"));
    assert!(
        loop_left >= outer_left + 2,
        "nested loop frame should not touch the parent frame border:\n{rendered}"
    );
    let submit_label = text_display_column(&rendered, "Submit job");
    let nested_label = text_display_column(&rendered, "GET /jobs/123");
    assert!(
        submit_label == nested_label,
        "nested loop body should keep participant lifelines aligned with the outer frame:\n{rendered}"
    );

    let rendered = render_sequence(input, &AsciiRenderOptions::unicode())
        .expect("nested loop inside alt should render as Unicode");
    let submit_label = text_display_column(&rendered, "Submit job");
    let nested_label = text_display_column(&rendered, "GET /jobs/123");
    assert!(
        submit_label == nested_label,
        "nested Unicode loop body should keep participant lifelines aligned with the outer frame:\n{rendered}"
    );
}