fn xychart_parser_header_only_preserves_empty_chart_state() {
    let rendered = render_xychart("xychart", &AsciiRenderOptions::ascii())
        .expect("empty xychart should render");

    assert_eq!(
        rendered,
        concat!(
            "xychart: empty\n",
            "orientation: vertical\n",
            "xAxis: band title(bytes=0)=\"\" categories=[]\n",
            "yAxis: linear title(bytes=0)=\"\" min=none max=none\n",
            "display: showTitle=true showDataLabel=false showDataLabelOutsideBar=false xAxis={showLabel=true showTitle=true showTick=true showAxisLine=true} yAxis={showLabel=true showTitle=true showTick=true showAxisLine=true}\n",
            "plots: []",
        )
    );
}