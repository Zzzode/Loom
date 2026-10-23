#[test]
fn subgraph_edges_borders_do_not_overlap() {
    let output = render_flowchart_fixture("subgraph_edges.mmd");
    let lines: Vec<&str> = output.lines().collect();

    let bottom_border_rows: Vec<usize> = lines
        .iter()
        .enumerate()
        .filter(|(_, line)| line.contains('└'))
        .map(|(index, _)| index)
        .collect();
    let top_border_rows: Vec<usize> = lines
        .iter()
        .enumerate()
        .filter(|(_, line)| line.contains('┌'))
        .map(|(index, _)| index)
        .collect();

    assert!(
        !bottom_border_rows.is_empty(),
        "Should have bottom border rows"
    );
    assert!(!top_border_rows.is_empty(), "Should have top border rows");

    let first_bottom = lines.iter().position(|line| line.contains('└')).unwrap();
    let second_top = lines
        .iter()
        .enumerate()
        .skip(first_bottom)
        .position(|(_, line)| line.contains('┌'))
        .map(|position| position + first_bottom);

    if let Some(second_top) = second_top {
        assert!(
            second_top > first_bottom,
            "Second subgraph top ({second_top}) should be below first subgraph bottom ({first_bottom})"
        );
    }
}