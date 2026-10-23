#[test]
fn backward_corridor_stays_valid_at_non_default_edge_label_spacing() {
    use crate::engines::graph::LayoutConfig as PublicLayoutConfig;
    use crate::{OutputFormat, RenderConfig};

    let src = "\
graph TD
    A --> B
    B -->|loop1| A
    C --> D
    D -->|loop2| C
";
    let cfg = RenderConfig {
        layout: PublicLayoutConfig {
            edge_label_spacing: 20.0,
            ..PublicLayoutConfig::default()
        },
        ..RenderConfig::default()
    };
    let rendered = crate::render_diagram(src, OutputFormat::Text, &cfg)
        .expect("render succeeds at non-default edge_label_spacing");

    assert!(
        rendered.lines().count() > 4,
        "expected multi-line render; got:\n{rendered}"
    );
    assert!(
        rendered.contains("loop1"),
        "loop1 label missing from render:\n{rendered}"
    );
    assert!(
        rendered.contains("loop2"),
        "loop2 label missing from render:\n{rendered}"
    );
}