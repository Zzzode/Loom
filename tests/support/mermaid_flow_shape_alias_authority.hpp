#pragma once
#include "mermaid_flow400_authority.hpp"

namespace loom::test::mermaid_flow {

struct FlowShapeAliasCase {
    std::size_t ordinal;
    std::size_t global;
    std::string_view fixture;
    std::size_t source_bytes;
    std::string_view source_sha;
    std::string_view source_git_blob;
    std::string_view model_json;
    std::string_view model_sha;
    std::array<std::string_view, 5> labels;
    std::size_t node_count;
    std::string_view shape;
};

inline constexpr std::array<FlowShapeAliasCase, 16> kFlowShapeAliasCases{{
    {153u, 814u,
     "flowchart/nightly-check/shape-alias/01-all-process-rect-proc-rectangle-should-render-same-shape.mmd",
     313u, "8c7eb8e1f00c03635ef90fda0dc45a0c2c91bfa193898af6403224aee0147dd0",
     "5f22a8c712990a314fae2797f26b224cfa124506",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"process","shape":"rectangle"}],["n1",{"id":"n1","label":"rect","shape":"rectangle"}],["n2",{"id":"n2","label":"proc","shape":"rectangle"}],["n3",{"id":"n3","label":"rectangle","shape":"rectangle"}]],"edges":[],"subgraphs":[]})MODEL",
     "258eccc2d1ae2d14338a36b9c5477fd994fcd4be213b9c5c4d658286990ef72f",
     {"process", "rect", "proc", "rectangle"}, 4u, "rectangle"},
    {154u, 815u,
     "flowchart/nightly-check/shape-alias/02-all-event-rounded-should-render-same-shape.mmd",
     226u, "a140c4fc10a50038356a415b7e23899d5d39db35e8ca76fd512dcf9b0fdd16d8",
     "d2c6daf1db8e3eb7d0447c7189bdb598faee8bd5",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"event","shape":"rounded"}],["n1",{"id":"n1","label":"rounded","shape":"rounded"}]],"edges":[],"subgraphs":[]})MODEL",
     "33a66ca70e33f7654cc24e243d01bd76e908f5a9a9d1835500c92a8b3dfbefd0",
     {"event", "rounded", "", ""}, 2u, "rounded"},
    {155u, 816u,
     "flowchart/nightly-check/shape-alias/03-all-stadium-pill-terminal-should-render-same-shape.mmd",
     264u, "5e52a9a22ef65b16b6e1769a1aeccd07518c505859b714ee6c225dd38b51e15b",
     "f235232e2aa8d6ebad611dc86144044efea7e7f4",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"stadium","shape":"stadium"}],["n1",{"id":"n1","label":"pill","shape":"stadium"}],["n2",{"id":"n2","label":"terminal","shape":"stadium"}]],"edges":[],"subgraphs":[]})MODEL",
     "bc4e7943ff5702712ce8737e22adaa5eb61c280e26bb173e92778b95f59534c2",
     {"stadium", "pill", "terminal", ""}, 3u, "stadium"},
    {156u, 817u,
     "flowchart/nightly-check/shape-alias/04-all-fr-rect-subproc-subprocess-framed-rectangle-subroutine-should-render-same-shape.mmd",
     423u, "d8152da1ba748eb7926493f4116d33daeabc1e2906beee0cbb378795b363d84d",
     "c7c2f2c4ec54044eeb429a4b75f6b5b62a6de488",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"fr-rect","shape":"subroutine"}],["n1",{"id":"n1","label":"subproc","shape":"subroutine"}],["n2",{"id":"n2","label":"subprocess","shape":"subroutine"}],["n3",{"id":"n3","label":"framed-rectangle","shape":"subroutine"}],["n4",{"id":"n4","label":"subroutine","shape":"subroutine"}]],"edges":[],"subgraphs":[]})MODEL",
     "ce9fa747bef598c8ecdd821c02d78723431d06f126de8f5ab67f4c2d1b3d2a2d",
     {"fr-rect", "subproc", "subprocess", "framed-rectangle", "subroutine"}, 5u, "subroutine"},
    {158u, 819u,
     "flowchart/nightly-check/shape-alias/06-all-diam-decision-diamond-should-render-same-shape.mmd",
     257u, "d73873cc38a21175cda899598aeb2896de43c79a92e8a5c0cc4130cc77f9e43c",
     "0064940762fb6335978c4f24419190a66cd00f9f",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"diam","shape":"diamond"}],["n1",{"id":"n1","label":"decision","shape":"diamond"}],["n2",{"id":"n2","label":"diamond","shape":"diamond"}]],"edges":[],"subgraphs":[]})MODEL",
     "e3c84c4ac2d9abd188bf478cb8dbc3e66aac908256787180988e14c00056760c",
     {"diam", "decision", "diamond", ""}, 3u, "diamond"},
    {159u, 820u,
     "flowchart/nightly-check/shape-alias/07-all-hex-hexagon-prepare-should-render-same-shape.mmd",
     264u, "da72601a7335d2e3f49b7fa5597f73f78f1d6cc36dc71b440df32dc1a610ae18",
     "863325257ec29a5a18621ae003d2858370d6b992",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"hex","shape":"hexagon"}],["n1",{"id":"n1","label":"hexagon","shape":"hexagon"}],["n2",{"id":"n2","label":"prepare","shape":"hexagon"}]],"edges":[],"subgraphs":[]})MODEL",
     "37ff7f06c4e36f2d86c4913b9cc515f7f94a6af44d04eeda02c18154749757ba",
     {"hex", "hexagon", "prepare"}, 3u, "hexagon"},
    {160u, 821u,
     "flowchart/nightly-check/shape-alias/08-all-lean-l-lean-left-out-in-should-render-same-shape.mmd",
     291u, "d2fc29f2a0dd0d31c8b8dc6035c59e63afa4537b4d431244c785910470899098",
     "95167a7120701336a547f355914504e58baa08ba",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"lean-l","shape":"parallelogram"}],["n1",{"id":"n1","label":"lean-left","shape":"parallelogram"}],["n2",{"id":"n2","label":"out-in","shape":"parallelogram"}]],"edges":[],"subgraphs":[]})MODEL",
     "31a11abfd23d83a136dd74b5e0de1037582b92112469da588ba80750780c5793",
     {"lean-l", "lean-left", "out-in"}, 3u, "parallelogram"},
    {161u, 822u,
     "flowchart/nightly-check/shape-alias/09-all-lean-r-lean-right-in-out-should-render-same-shape.mmd",
     295u, "9593d0c9d2ac96424e9a0f60271a14b4bc0dd1484ced13708c9f061868b60f6b",
     "45f1dd0f9ad5ada3904d80df2839b768d33ff207",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"lean-r","shape":"parallelogram_alt"}],["n1",{"id":"n1","label":"lean-right","shape":"parallelogram_alt"}],["n2",{"id":"n2","label":"in-out","shape":"parallelogram_alt"}]],"edges":[],"subgraphs":[]})MODEL",
     "79ba076889283aee17c1a9ace32cf006c13be647f447e0182c466667b2269c33",
     {"lean-r", "lean-right", "in-out"}, 3u, "parallelogram_alt"},
    {162u, 823u,
     "flowchart/nightly-check/shape-alias/10-all-trap-b-trapezoid-bottom-priority-should-render-same-shape.mmd",
     311u, "e1bd0381245d693f5063a5c53996ddbfa1aca2d480b9175529582d50c206d8f2",
     "36a9c24faad130eb13f1399f158169b1782d6384",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"trap-b","shape":"trapezoid"}],["n1",{"id":"n1","label":"trapezoid-bottom","shape":"trapezoid"}],["n2",{"id":"n2","label":"priority","shape":"trapezoid"}]],"edges":[],"subgraphs":[]})MODEL",
     "121befd7eac795fd2cea16d248d58e218008fcd01c008621abfcce096fc1aab7",
     {"trap-b", "trapezoid-bottom", "priority"}, 3u, "trapezoid"},
    {163u, 824u,
     "flowchart/nightly-check/shape-alias/11-all-trap-t-trapezoid-top-manual-should-render-same-shape.mmd",
     294u, "ad6b72b819859b7de4a933924a8c86e7deb24b90986832ffedbdb22aede34473",
     "e87228445d0fc26862afe021e6536604d939a82a",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"trap-t","shape":"trapezoid_alt"}],["n1",{"id":"n1","label":"trapezoid-top","shape":"trapezoid_alt"}],["n2",{"id":"n2","label":"manual","shape":"trapezoid_alt"}]],"edges":[],"subgraphs":[]})MODEL",
     "76b25dd1554f9540edfaf80d58dfcf79699df3d879d54d8113387ad89bb75960",
     {"trap-t", "trapezoid-top", "manual"}, 3u, "trapezoid_alt"},
    {164u, 825u,
     "flowchart/nightly-check/shape-alias/12-all-dbl-circ-double-circle-should-render-same-shape.mmd",
     235u, "66474010cc7bb7eac05369d2496ce19c9a242715febe339019024d02b947f354",
     "d34cb5eecdcc279c0cbd0ea7fc4cdcdee39017ce",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"dbl-circ","shape":"double_circle"}],["n1",{"id":"n1","label":"double-circle","shape":"double_circle"}]],"edges":[],"subgraphs":[]})MODEL",
     "d67813b6203ea60c407653b4316937812af5fd165c988a97d20f03a9e70f38b9",
     {"dbl-circ", "double-circle"}, 2u, "double_circle"},
    {157u, 818u,
     "flowchart/nightly-check/shape-alias/05-all-db-database-cylinder-cyl-should-render-same-shape.mmd",
     303u, "6871dfec47253ed459936de3cad35cc544bdd16e79bdd5d06031faa06b830c60",
     "fd875da5685359af007d56b4efae177416cc5bc1",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"db","shape":"cylinder"}],["n1",{"id":"n1","label":"database","shape":"cylinder"}],["n2",{"id":"n2","label":"cylinder","shape":"cylinder"}],["n3",{"id":"n3","label":"cyl","shape":"cylinder"}]],"edges":[],"subgraphs":[]})MODEL",
     "0f9d08a5a9da7ac62a66a706311f6bfeb2a074929712388cb0f09e0770ec62b3",
     {"db", "database", "cylinder", "cyl"}, 4u, "cylinder"},
    {166u, 827u,
     "flowchart/nightly-check/shape-alias/14-all-sm-circ-small-circle-start-should-render-same-shape.mmd",
     280u, "d6e1d4f699697538b7e6ef9198d28c94b3d364d13233f6b0e7c1413f3e6d7452",
     "1b79431bc15b50f5e70449d6d7aff598cead03d3",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"sm-circ","shape":"small_circle"}],["n1",{"id":"n1","label":"small-circle","shape":"small_circle"}],["n2",{"id":"n2","label":"start","shape":"small_circle"}]],"edges":[],"subgraphs":[]})MODEL",
     "dc62a71cea41c7c18be9b9f8448afbe73617fd4f70d1736846754645e277aa1d",
     {"sm-circ", "small-circle", "start", ""}, 3u, "small_circle"},
    {170u, 831u,
     "flowchart/nightly-check/shape-alias/18-all-fork-join-should-render-same-shape.mmd",
     198u, "677b29a437c2c1c0b4796e124e721231633fb2226e4237ff0fad61222071d43d",
     "e545b78e98198bc56bf6d8bd07ffabcb5f62b9f9",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"fork","shape":"fork_join"}],["n1",{"id":"n1","label":"join","shape":"fork_join"}]],"edges":[],"subgraphs":[]})MODEL",
     "2c6a0a0694d8b5e6aecfaa0941d3eadc7ae391a10524191b5ee258479d50f88b",
     {"fork", "join", "", ""}, 2u, "fork_join"},
    {173u, 834u,
     "flowchart/nightly-check/shape-alias/21-all-delay-half-rounded-rectangle-should-render-same-shape.mmd",
     257u, "3a797c37a4a4450f4a7bed35db0eba70e4c1e6cdf209e1f5393c232693b15635",
     "6691703b4af36613e2a8d9309bd00f061d97146a",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"delay","shape":"delay"}],["n1",{"id":"n1","label":"half-rounded-rectangle","shape":"delay"}]],"edges":[],"subgraphs":[]})MODEL",
     "ca65bbebab86fc7a14ae60ea51fe91c5d63af553b1ab58a252a39ff37252eeb9",
     {"delay", "half-rounded-rectangle", "", ""}, 2u, "delay"},
    {178u, 839u,
     "flowchart/nightly-check/shape-alias/26-all-extract-tri-triangle-should-render-same-shape.mmd",
     254u, "af83d33152ee9d3dbd6a014a8059483d34965478dcd8c55c8cc5ff7f5f22856b",
     "98bfbc37ad26cc21e647e5ad273882d4bc4d4506",
     R"MODEL({"direction":"TB","nodes":[["n0",{"id":"n0","label":"extract","shape":"triangle"}],["n1",{"id":"n1","label":"tri","shape":"triangle"}],["n2",{"id":"n2","label":"triangle","shape":"triangle"}]],"edges":[],"subgraphs":[]})MODEL",
     "f5915d1e83b135ae29d36023dbc9040d02cdcbffd7ab4ed000731e3ed7d5adc1",
     {"extract", "tri", "triangle"}, 3u, "triangle"},
}};

struct FlowShapeAliasBodyCase {
    std::size_t ordinal;
    std::size_t global;
    std::size_t offset;
    std::string_view sha;
    std::string_view body;
    std::string_view witness;
    std::string_view snapshot;
    std::string_view snapshot_sha;
};

inline constexpr std::array<FlowShapeAliasBodyCase, 14> kFlowShapeAliasBodyCases{{
    {153u, 814u, 146u, "46b9441910056321e6eee67f4876270cf299bfdd1285f213cfbfd2a587d84be2",
     "flowchart \n n0@{ shape: process, label: \"process\" }\n n1@{ shape: rect, label: \"rect\" }\n n2@{ shape: proc, label: \"proc\" }\n n3@{ shape: rectangle, label: \"rectangle\" }\n",
     "n0@{ shape: process, label: \"process\" }", "flow400_shape_alias_process_body.txt",
     "14bcf64ecba0dc2f2d8cfbf330ef9df681e34691219a82ecc76ef4af7155197f"},
    {155u, 816u, 134u, "8b7b7087514c7be5b4287fc7ba8c9f43132537fb65a141b013f9718c7cb6d745",
     "flowchart \n n0@{ shape: stadium, label: \"stadium\" }\n n1@{ shape: pill, label: \"pill\" }\n n2@{ shape: terminal, label: \"terminal\" }\n",
     "n0@{ shape: stadium, label: \"stadium\" }", "flow400_shape_alias_stadium_body.txt",
     "23bbd742704a0096e3e52d6a0b344b1f3c5b18a780a4c42faa3ff7979e710ae5"},
    {156u, 817u, 177u, "3fa6acc313d10c9edd5b59f174e2a98f822b3d755c55b1d872e54abb4f70b669",
     "flowchart \n n0@{ shape: fr-rect, label: \"fr-rect\" }\n n1@{ shape: subproc, label: \"subproc\" }\n n2@{ shape: subprocess, label: \"subprocess\" }\n n3@{ shape: framed-rectangle, label: \"framed-rectangle\" }\n n4@{ shape: subroutine, label: \"subroutine\" }\n",
     "n0@{ shape: fr-rect, label: \"fr-rect\" }", "flow400_shape_alias_subroutine_body.txt",
     "ba47e616d5df96dcc0f0010530ae4faefb7ebcdb410620335a70e77bbd3e46c5"},
    {157u, 818u, 142u, "c710fd51b3dbc394ac61b0965fad260125c810bf8d52b5a610e74a948f107f42",
     "flowchart \n n0@{ shape: db, label: \"db\" }\n n1@{ shape: database, label: \"database\" }\n n2@{ shape: cylinder, label: \"cylinder\" }\n n3@{ shape: cyl, label: \"cyl\" }\n",
     "n0@{ shape: db, label: \"db\" }", "flow400_shape_alias_cylinder_body.txt",
     "ff07ac92b24779c3a2e60fee94aa00510ad7192829c4c77b2ccab2a18321cfee"},
    {159u, 820u, 138u, "579af2dd9068b2409d10b14c16ebf946939b9c9ae2574d6ef3386d396f5deb9d",
     "flowchart \n n0@{ shape: hex, label: \"hex\" }\n n1@{ shape: hexagon, label: \"hexagon\" }\n n2@{ shape: prepare, label: \"prepare\" }\n",
     "n0@{ shape: hex, label: \"hex\" }", "flow400_shape_alias_hexagon_body.txt",
     "43a585b629bad09d516861bdf4448fa1cab2c898bc64d0d80b95799764d0d38a"},
    {160u, 821u, 157u, "c1aeedea49081397d3215953140a42467b5b5991d25683e5e9f7a8d019660b6b",
     "flowchart \n n0@{ shape: lean-l, label: \"lean-l\" }\n n1@{ shape: lean-left, label: \"lean-left\" }\n n2@{ shape: out-in, label: \"out-in\" }\n",
     "n0@{ shape: lean-l, label: \"lean-l\" }", "flow400_shape_alias_parallelogram_body.txt",
     "860bf7499d01b5a3a5d12f672ae45fc806f5fca61e8b893a150033e0ccba23d8"},
    {161u, 822u, 159u, "e3838df64132af08d59f5c4f11017f70ac58b0988a5c25bd3dd356cb071c37d8",
     "flowchart \n n0@{ shape: lean-r, label: \"lean-r\" }\n n1@{ shape: lean-right, label: \"lean-right\" }\n n2@{ shape: in-out, label: \"in-out\" }\n",
     "n0@{ shape: lean-r, label: \"lean-r\" }", "flow400_shape_alias_parallelogram_alt_body.txt",
     "1be8bb196c772fc5f8fc37e7577763e65699414b63895d71acbab4c058b35cc2"},
    {162u, 823u, 159u, "627dbbcb27fbc5293be8a3bfaac4c4f095939dc716a77637a10dd11aa31f6758",
     "flowchart \n n0@{ shape: trap-b, label: \"trap-b\" }\n n1@{ shape: trapezoid-bottom, label: \"trapezoid-bottom\" }\n n2@{ shape: priority, label: \"priority\" }\n",
     "n0@{ shape: trap-b, label: \"trap-b\" }", "flow400_shape_alias_trapezoid_body.txt",
     "87c593776094050017a111f9b879c429d918aab7e969fe180204e452469ce057"},
    {163u, 824u, 152u, "39e8e84380a4105b731bfada788c3e1109a77d95daa166d795fd74c796273d3b",
     "flowchart \n n0@{ shape: trap-t, label: \"trap-t\" }\n n1@{ shape: trapezoid-top, label: \"trapezoid-top\" }\n n2@{ shape: manual, label: \"manual\" }\n",
     "n0@{ shape: trap-t, label: \"trap-t\" }", "flow400_shape_alias_trapezoid_alt_body.txt",
     "793c396a6d9a4bd1a19cd2f7ac92e764eb7557099c8f4c43dfc7350511310b77"},
    {164u, 825u, 128u, "c5cb4f5511e1afda8890736ae898c28d7f9ffef6aae635f82ade05169f748172",
     "flowchart \n n0@{ shape: dbl-circ, label: \"dbl-circ\" }\n n1@{ shape: double-circle, label: \"double-circle\" }\n",
     "n0@{ shape: dbl-circ, label: \"dbl-circ\" }", "flow400_shape_alias_double_circle_body.txt",
     "18391c97d4682b85d82c9c7ac3fd031c8443edcbab6668d710a7b0a9d2e42e59"},
    {166u, 827u, 140u, "3bf4d519a70bfe1c465d4eaa9399a86483b7550934233b39039c884ec29aa047",
     "flowchart \n n0@{ shape: sm-circ, label: \"sm-circ\" }\n n1@{ shape: small-circle, label: \"small-circle\" }\n n2@{ shape: start, label: \"start\" }\n",
     "n0@{ shape: sm-circ, label: \"sm-circ\" }", "flow400_shape_alias_small_circle_body.txt",
     "b7a3767d8180d39ab4928f7824f58746569ca0c214382816b1ff6ee171ab5296"},
    {170u, 831u, 117u, "e38b5837785189b47b38f307df04ca7ff76f8a77de7a7bc29d58678979deaf7f",
     "flowchart \n n0@{ shape: fork, label: \"fork\" }\n n1@{ shape: join, label: \"join\" }\n",
     "n0@{ shape: fork, label: \"fork\" }", "flow400_shape_alias_fork_join_body.txt",
     "ee959a75ce1d1266fb7f5ea9f8ef4e325ecade963b53ae6dbc55375e30428021"},
    {173u, 834u, 138u, "b7c9b27a7b8e4ae413418f0dafe907a99571f972581ba3d677f43c2f48319084",
     "flowchart \n n0@{ shape: delay, label: \"delay\" }\n n1@{ shape: half-rounded-rectangle, label: \"half-rounded-rectangle\" }\n",
     "n0@{ shape: delay, label: \"delay\" }", "flow400_shape_alias_delay_body.txt",
     "20fbb0fc955d82119f4c6411e0a11adedf35912ec1b3c24d96438ccad17409d8"},
    {178u, 839u, 126u, "c37580d6c301b321b0e516e98551f9b9afe213e53f8adb81bd45f57f3b457cbd",
     "flowchart \n n0@{ shape: extract, label: \"extract\" }\n n1@{ shape: tri, label: \"tri\" }\n n2@{ shape: triangle, label: \"triangle\" }\n",
     "n0@{ shape: extract, label: \"extract\" }", "flow400_shape_alias_triangle_body.txt",
     "b388c50c6b35b447a77747ceea3060b4f2408733f963ac0f0cfe8ab35f6b1221"},
}};

inline const FlowShapeAliasBodyCase* flow_shape_alias_body_case(std::size_t ordinal) {
    const auto found = std::ranges::find_if(kFlowShapeAliasBodyCases,
        [&](const FlowShapeAliasBodyCase& candidate) { return candidate.ordinal == ordinal; });
    return found == kFlowShapeAliasBodyCases.end() ? nullptr : &*found;
}

inline const FlowShapeAliasCase* flow_shape_alias_case(std::size_t ordinal) {
    const auto found = std::ranges::find_if(kFlowShapeAliasCases,
        [&](const FlowShapeAliasCase& candidate) { return candidate.ordinal == ordinal; });
    return found == kFlowShapeAliasCases.end() ? nullptr : &*found;
}

inline void flow_shape_alias_body_bind(JsonVal fixture, JsonVal diagnostic,
                                       std::size_t ordinal, std::size_t global,
                                       std::string_view source, std::string_view body) {
    const auto* selected = flow_shape_alias_case(ordinal);
    const auto* body_case = flow_shape_alias_body_case(ordinal);
    flow400_require(selected != nullptr && body_case != nullptr && global == selected->global &&
                    global == body_case->global &&
                    flow400_text(fixture.get("path")) == selected->fixture &&
                    flow400_number(fixture.get("bytes")) == selected->source_bytes &&
                    flow400_text(fixture.get("sha256")) == selected->source_sha &&
                    flow400_text(fixture.get("git_blob")) == selected->source_git_blob,
                    "Flow shape-alias body Official manifest tuple");
    flow400_require(source.size() == selected->source_bytes && digest(source) == selected->source_sha,
                    "Flow shape-alias body complete source binding");
    flow400_require(flow400_text(diagnostic.get("expected")) == "source" &&
                    flow400_text(diagnostic.get("reason")) == "inline-config" &&
                    flow400_number(diagnostic.get("offset")) == body_case->offset &&
                    flow400_text(diagnostic.get("sha256")) == body_case->sha,
                    "Flow shape-alias body original diagnostic expectation");
    const auto witness = diagnostic.get("witness");
    flow400_require(flow400_number(witness.get("offset")) == 12u &&
                    flow400_text(witness.get("text")) == body_case->witness,
                    "Flow shape-alias body diagnostic witness");
    flow400_require(source.substr(body_case->offset) == body_case->body &&
                    body == body_case->body && body.size() == body_case->body.size() &&
                    digest(body) == body_case->sha && digest(selected->model_json) == selected->model_sha,
                    "Flow shape-alias exact diagnostic body and Model bytes");
}

inline void flow_shape_alias_bind(const Flow400Documents& docs, JsonVal fixture,
                                  std::size_t ordinal, std::size_t global,
                                  std::string_view source, OracleWork& work) {
    const auto* selected = flow_shape_alias_case(ordinal);
    flow400_require(selected != nullptr && selected->global == global,
                    "Flow shape-alias authority ordinal tuple");
    flow400_require(flow400_text(fixture.get("path")) == selected->fixture &&
                    flow400_number(fixture.get("bytes")) == selected->source_bytes &&
                    flow400_text(fixture.get("sha256")) == selected->source_sha &&
                    flow400_text(fixture.get("git_blob")) == selected->source_git_blob,
                    "Flow shape-alias Official manifest tuple");
    flow400_bind(docs.compact.root().get("cases").at(ordinal), fixture, ordinal, global,
                 source, work);
    flow400_require(source.size() == selected->source_bytes && digest(source) == selected->source_sha,
                    "Flow shape-alias complete source binding");
    flow400_require(digest(selected->model_json) == selected->model_sha,
                    "Flow shape-alias canonical Model binding");
}

inline Model flow_shape_alias_model(std::size_t ordinal, OracleWork& work) {
    const auto* selected = flow_shape_alias_case(ordinal);
    flow400_require(selected != nullptr, "Flow shape-alias model ordinal");
    work.product(selected->model_json.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto document = json::parse(selected->model_json);
    flow400_require(document.has_value(), "Flow shape-alias Model JSON");
    std::size_t groups = 0u, members = 0u;
    flow400_shape(document->root(), groups, members, true);
    flow400_require(groups == 0u && members == 0u &&
                    document->root().get("nodes").size() == selected->node_count &&
                    document->root().get("edges").size() == 0u,
                    "Flow shape-alias Model cardinality");
    const auto nodes = document->root().get("nodes");
    for (std::size_t index = 0u; index < selected->node_count; ++index) {
        const auto node = nodes.at(index).at(1u);
        flow400_require(flow400_text(node.get("id")) == "n" + std::to_string(index) &&
                        flow400_text(node.get("label")) == selected->labels[index] &&
                        flow400_text(node.get("shape")) == selected->shape,
                        "Flow shape-alias Model node identity");
    }
    work.spend(4u * sizeof(Model) + 512u);
    work.product(selected->node_count, 8u * sizeof(Node) + 64u);
    return parse_model(document->root());
}

} // namespace loom::test::mermaid_flow
