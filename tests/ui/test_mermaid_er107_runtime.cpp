// Same-factory feasibility only. Drawing admission belongs to independently
// captured Native and Official consumers, never to this boolean test seam.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
import std;
import loom.ui.visual.mermaid.testing;
import loom.serdes.json;
import loom.crypto.crypto;
#include "mermaid_er107_source.hpp"

namespace {
namespace er=loom::test::mermaid_er107;
namespace mt=loom::ui::mermaid::testing;
std::string raw(std::string_view path) {
    const auto file=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_official/cases"/path;
    if(!std::filesystem::is_regular_file(file)||std::filesystem::is_symlink(file)||std::filesystem::file_size(file)>65536u)throw std::runtime_error("ER107 bounded raw input");
    std::ifstream stream(file,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
bool succeeds(std::string_view source){mt::Work work;return mt::layout_succeeds(source,false,work);}
}
TEST(MermaidER107Runtime, CompleteOriginalsUseSameFactoryOneMillionBudget) {
    std::size_t successes=0,maximum=0;
    for(const auto& pin:er::kPins) {
        const auto source=raw(pin.path);mt::Work work;
        const bool result=mt::layout_succeeds(source,false,work);
        const auto used=mt::kWorkLimit-work.remaining;
        std::cout<<"ER107_FACTORY "<<pin.path<<" ok="<<result<<" used="<<used<<'\n';
        EXPECT_TRUE(result)<<pin.path<<" used="<<used;
        if(!result)continue;
        ++successes;maximum=std::max(maximum,used);
        mt::Work exact{used};EXPECT_TRUE(mt::layout_succeeds(source,false,exact))<<pin.path;
        EXPECT_EQ(exact.remaining,0u)<<pin.path;
        ASSERT_GT(used,0u);
        mt::Work short_one{used-1u};EXPECT_FALSE(mt::layout_succeeds(source,false,short_one))<<pin.path;
    }
    std::cout<<"ER107_FACTORY_SUMMARY success="<<successes<<" max="<<maximum<<'\n';
    EXPECT_EQ(successes,107u);
}
TEST(MermaidER107Runtime, ClosedNewProfileRejectsIncompleteProjection) {
    const std::string prefix="---\nconfig:\n  look: neo\n---\nerDiagram\n";
    EXPECT_TRUE(succeeds(prefix+"A {\n type~T~ id\n}\n"));
    for(const auto type:{"type~T~x","type~T,U~","type~~","type~T~U~","type~T"})
        EXPECT_FALSE(succeeds(prefix+"A {\n "+type+" id\n}\n"))<<type;
    EXPECT_TRUE(succeeds(prefix+"A ||--o{ B : \"  owns  \"\n"));
    EXPECT_TRUE(succeeds(prefix+"A ||--o{ B : \"   \"\n"));
    EXPECT_TRUE(succeeds(prefix+"DEAD_PARROT {\n type~T~ *id\n}\n"));
    EXPECT_TRUE(succeeds("---\nconfig:\n  look: neo\n  er:\n    useMaxWidth: false\n---\nerDiagram\nA\n"));
    EXPECT_FALSE(succeeds("---\nconfig:\n  look: neo\n  er.useMaxWidth: false\n---\nerDiagram\nA\n"));
    EXPECT_FALSE(succeeds("---\nconfig:\n  look: neo\n  er:\n  er.useMaxWidth: false\n---\nerDiagram\nA\n"));
    for(const auto body:{"A[\"_caption_\"]\n","A[\"[caption](url)\"]\n","A[\"![](url)\"]\n","A[\"# heading\"]\n","A[\"- item\"]\n","A {\n int na~me~\n}\n","A {\n int id \"<strong>x</strong>\"\n}\n","A:::x\n","A\nclassDef x fill:red\n","A[\"<br>\"]\n"})
        EXPECT_FALSE(succeeds(prefix+body))<<body;
    for(const auto config:{"  look: unknown\n","  htmlLabels: true\n","  look: neo\n  theme: forest\n","  look: neo\n  fontSize: 12\n","  look: neo\n  unknown: false\n","  look: neo\n  er: {}\n"})
        EXPECT_FALSE(succeeds(std::string("---\nconfig:\n")+config+"---\nerDiagram\nA\n"))<<config;
    EXPECT_FALSE(succeeds("---\ntitle: unreviewed\nconfig:\n  look: neo\n---\nerDiagram\nA\n"));
}
TEST(MermaidER107Runtime, ExistingFamilyDispatchAndBudgetExhaustionRemainAtomic) {
    for(const auto source:{"erDiagram\nA ||--o{ B : has\n","classDiagram\nA --> B\n","stateDiagram-v2\nA --> B\n","flowchart LR\nA --> B\n"}) {
        EXPECT_TRUE(succeeds(source))<<source;
        mt::Work zero{0};EXPECT_FALSE(mt::layout_succeeds(source,false,zero));
    }
    mt::Work work;EXPECT_FALSE(mt::layout_succeeds("",false,work));
}
