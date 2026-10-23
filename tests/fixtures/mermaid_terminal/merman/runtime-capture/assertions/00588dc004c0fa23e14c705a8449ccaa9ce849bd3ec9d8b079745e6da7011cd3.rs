fn graph_fixture_named_gaps_preserve_visible_text_and_render() {
    let mut failures = Vec::new();
    for fixture in GRAPH_FIXTURE_GAPS {
        let path = fixture_path(*fixture);
        let (input, expected) = split_fixture(&path);
        match render_flowchart(&input, &fixture.options()) {
            Ok(rendered) if !rendered.trim().is_empty() => {
                let expected_tokens = visible_text_token_counts(&expected);
                let rendered_tokens = visible_text_tokens(&rendered);
                for (token, expected_count) in expected_tokens {
                    let actual_count = visible_token_occurrences(&rendered_tokens, &token);
                    if actual_count < expected_count {
                        failures.push(format!(
                            "{}: visible token {token:?} occurs {actual_count} times, expected at least {expected_count}\n{rendered}",
                            fixture.key(),
                        ));
                    }
                }
            }
            Ok(_) => failures.push(format!("{}: empty output", fixture.key())),
            Err(error) => failures.push(format!("{}: {error}", fixture.key())),
        }
    }

    assert!(
        failures.is_empty(),
        "named graph fixture gaps must remain renderable:\n{}",
        failures.join("\n")
    );
}