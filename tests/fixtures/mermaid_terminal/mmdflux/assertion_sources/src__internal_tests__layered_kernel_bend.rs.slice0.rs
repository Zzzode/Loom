#[test]
fn text_render_honors_edge_label_spacing() {
    use crate::engines::graph::LayoutConfig as PublicLayoutConfig;
    use crate::{OutputFormat, RenderConfig};

    let src = "graph TD\n    A -->|label| B\n";
    let mk = |spacing: f64| RenderConfig {
        layout: PublicLayoutConfig {
            edge_label_spacing: spacing,
            ..PublicLayoutConfig::default()
        },
        ..RenderConfig::default()
    };

    let text_small = crate::render_diagram(src, OutputFormat::Text, &mk(2.0)).unwrap();
    let text_big = crate::render_diagram(src, OutputFormat::Text, &mk(40.0)).unwrap();

    assert_ne!(
        text_small, text_big,
        "edge_label_spacing must visibly affect Text output"
    );

    let small_rows = text_small.lines().count();
    let big_rows = text_big.lines().count();
    assert!(
        big_rows > small_rows + 1,
        "increasing edge_label_spacing must add rows between labeled ranks: small={small_rows}, big={big_rows}"
    );
}