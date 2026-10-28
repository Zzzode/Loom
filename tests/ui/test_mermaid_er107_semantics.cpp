// ER107 B1 source-binding preflight only. No production module, ER parser,
// renderer, captured pixels, terminal Model or admission updater is imported.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
import std;
import loom.serdes.json;
import loom.crypto.crypto;
#include "mermaid_er107_source.hpp"

namespace {
namespace er = loom::test::mermaid_er107;
namespace json = loom::utils::json;
namespace fs = std::filesystem;
const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures";

std::string read(const fs::path& path, std::size_t cap = er::kMaxFactBytes) {
    er::detail::need(!fs::is_symlink(fs::symlink_status(path)) && fs::is_regular_file(path) &&
                         fs::file_size(path) <= cap,
                     "resource: ER107 bounded source file");
    std::ifstream stream(path, std::ios::binary);
    er::detail::need(stream.good(), "protocol: ER107 source file open");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct Inputs {
    std::string source_bytes = read(kFixtures / std::string(er::kFactsPath));
    std::string manifest_bytes = read(kFixtures / "mermaid_official/manifest.json");
    std::string prior_bytes = read(kFixtures / std::string(er::kPriorPath));
    json::JsonDoc source, manifest, prior;
    std::optional<er::Assets> assets;

    Inputs() {
        er::OracleWork bytes_work;
        er::verify_asset_bytes(source_bytes, manifest_bytes, prior_bytes, bytes_work);
        auto s = json::parse(source_bytes), m = json::parse(manifest_bytes),
             p = json::parse(prior_bytes);
        er::detail::need(s && m && p, "protocol: ER107 source JSON parse");
        source = std::move(*s);
        manifest = std::move(*m);
        prior = std::move(*p);
        er::OracleWork root_work;
        assets.emplace(er::verify_roots(source.root(), manifest.root(), prior.root(), root_work));
    }

    std::string raw(const er::Pin& pin) const {
        return read(kFixtures / "mermaid_official/cases" / std::string(pin.path), er::kMaxRawBytes);
    }

    er::BoundFacts bind(const er::Pin& pin, std::string_view bytes, er::OracleWork& work) const {
        return er::bind(*assets, pin.path, bytes, work);
    }
};

template <class F> void rejects_without_resource(F&& action, std::string_view reason = {}) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception& error) {
        const std::string_view text = error.what();
        EXPECT_FALSE(text.starts_with("resource:")) << text;
        if (!reason.empty()) EXPECT_NE(text.find(reason), std::string_view::npos) << text;
        rejected = true;
    }
    EXPECT_TRUE(rejected) << reason;
}

json::JsonMutDoc copy(json::JsonVal value) {
    json::JsonMutDoc d;
    d.set_root(d.copy_val(value));
    return d;
}

json::JsonDoc parsed(json::JsonMutDoc& value) {
    auto p = json::parse(value.to_string());
    er::detail::need(p.has_value(), "protocol: ER107 control JSON parse");
    return std::move(*p);
}
}  // namespace

TEST(MermaidEr107SourcePreflight, All107CompleteOriginalsBindWithoutTerminalAdmission) {
    Inputs input;
    std::size_t raw_bytes = 0, witnesses = 0, entities = 0, attributes = 0, relationships = 0,
                groups = 0, leaves = 0;
    std::set<std::string> paths, git_blobs;
    std::size_t smallest_remaining = 512'000'000;
    for (const auto& pin : er::kPins) {
        SCOPED_TRACE(pin.path);
        const auto raw = input.raw(pin);
        er::OracleWork work;
        const auto facts = input.bind(pin, raw, work);
        EXPECT_EQ(facts.path, pin.path);
        EXPECT_EQ(facts.global, pin.global);
        EXPECT_TRUE(paths.insert(facts.path).second);
        git_blobs.insert(std::string(pin.git_blob));
        raw_bytes += raw.size();
        witnesses += facts.source_witnesses.size();
        const auto model = facts.source_model, es = model.get("entities");
        entities += es.size();
        for (std::size_t i = 0; i < es.size(); ++i)
            attributes += es.at(i).get("attributes").size();
        relationships += model.get("relationships").size();
        groups += model.get("subgraphs").size();
        leaves += model.get("metadata").get("ordered_config_leaves").size();
        EXPECT_TRUE(facts.source_case.get("terminal_model").is_null());
        EXPECT_TRUE(model.get("terminal_model").is_null());
        EXPECT_EQ(facts.source_case.get_int("drawing_credit"), 0);
        EXPECT_EQ(facts.source_case.get_string("review_status"), "UNREVIEWED");
        smallest_remaining = std::min(smallest_remaining, work.left);
    }
    EXPECT_EQ(paths.size(), 107u);
    EXPECT_EQ(raw_bytes, 43704u);
    EXPECT_EQ(witnesses, 6616u);
    EXPECT_EQ(entities, 362u);
    EXPECT_EQ(attributes, 287u);
    EXPECT_EQ(relationships, 253u);
    EXPECT_EQ(groups, 1u);
    EXPECT_EQ(leaves, 181u);
    EXPECT_FALSE(git_blobs.empty());
    EXPECT_GT(smallest_remaining, 0u);
    // This is a project-native source binding result, not localReady drawing,
    // terminal portability or an automatic change to the 688 formal originals.
}

TEST(MermaidEr107SourcePreflight, GoodFirstRawWitnessConfigAndRecomputedModelMutantsReject) {
    Inputs input;
    const auto& pin = er::kPins.front();
    const auto raw = input.raw(pin);
    er::OracleWork good;
    const auto bound = input.bind(pin, raw, good);
    ASSERT_EQ(bound.path, pin.path);
    {
        er::OracleWork work;
        er::detail::witness_slices(bound.source_case, raw, work);
    }
    rejects_without_resource(
        [&] {
            er::OracleWork work;
            static_cast<void>(input.bind(pin, raw + " ", work));
        },
        "raw complete byte count");
    auto changed_raw = raw;
    changed_raw.back() = changed_raw.back() == ' ' ? '!' : ' ';
    rejects_without_resource(
        [&] {
            er::OracleWork work;
            static_cast<void>(input.bind(pin, changed_raw, work));
        },
        "raw complete SHA");
    rejects_without_resource(
        [&] {
            er::OracleWork work;
            static_cast<void>(er::bind(*input.assets, "er-diagram/not-a-member.mmd", raw, work));
        },
        "outside exact107");
    // Probe exact slice/line guards directly after their good baseline, so a
    // top-level SHA rejection cannot substitute for exercising those checks.
    for (const auto key : {"source_byte_end", "source_line_start", "source_slice_sha256"}) {
        auto doc = copy(bound.source_case);
        auto root = doc.root_mut();
        auto witness = root.get("source_witnesses").at(0);
        if (std::string_view(key) == "source_slice_sha256")
            witness.set(key, std::string(64, '0'));
        else
            witness.set(key, witness.get(key).as_int() + 1);
        auto immutable = parsed(doc);
        rejects_without_resource(
            [&] {
                er::OracleWork work;
                er::detail::witness_slices(immutable.root(), raw, work);
            },
            "ER107 witness");
    }
    {
        auto doc = copy(bound.source_case);
        auto root = doc.root_mut();
        auto model = root.get("source_model");
        model.get("metadata").get("ordered_config_leaves").at(0).set("source_value", true);
        root.get("prior_processed_config_post_addDirective").get("er").set("useMaxWidth", true);
        auto immutable = parsed(doc);
        rejects_without_resource(
            [&] {
                er::OracleWork work;
                er::detail::witness_slices(immutable.root(), raw, work);
            },
            "exact authored config scalar");
    }
    {
        auto doc = copy(input.source.root());
        auto root = doc.root_mut();
        auto c = root.get("cases").at(0);
        c.get("source_model").get("entities").at(0).set("source_db_label", "FORGED");
        auto interim = parsed(doc);
        er::OracleWork work;
        c.set("source_model_sha256",
              loom::utils::crypto::sha256(er::detail::canonical(
                  interim.root().get("cases").at(0).get("source_model"), work)));
        auto mutant = parsed(doc);
        rejects_without_resource(
            [&] {
                er::OracleWork w;
                static_cast<void>(
                    er::verify_roots(mutant.root(), input.manifest.root(), input.prior.root(), w));
            },
            "immutable declared component pin");
    }
    {
        auto doc = copy(input.source.root());
        doc.root_mut().get("cases").at(0).set("terminal_model", true);
        auto mutant = parsed(doc);
        rejects_without_resource(
            [&] {
                er::OracleWork w;
                static_cast<void>(
                    er::verify_roots(mutant.root(), input.manifest.root(), input.prior.root(), w));
            },
            "terminal field must remain null");
    }
    {
        auto doc = copy(input.prior.root());
        doc.root_mut()
            .get("cases")
            .at(static_cast<std::size_t>(pin.prior_ordinal))
            .set("source_git_blob", std::string(40, '0'));
        auto mutant = parsed(doc);
        rejects_without_resource(
            [&] {
                er::OracleWork w;
                static_cast<void>(
                    er::verify_roots(input.source.root(), input.manifest.root(), mutant.root(), w));
            },
            "immutable full prior root");
    }
    {
        auto doc = copy(input.manifest.root());
        doc.root_mut()
            .get("fixtures")
            .at(static_cast<std::size_t>(pin.global))
            .set("git_blob", std::string(40, '0'));
        auto mutant = parsed(doc);
        rejects_without_resource(
            [&] {
                er::OracleWork w;
                static_cast<void>(
                    er::verify_roots(input.source.root(), mutant.root(), input.prior.root(), w));
            },
            "immutable full manifest root");
    }
    rejects_without_resource(
        [&] {
            er::OracleWork w;
            er::verify_asset_bytes(input.source_bytes + " ", input.manifest_bytes,
                                   input.prior_bytes, w);
        },
        "source asset raw SHA");
    er::OracleWork exhausted;
    exhausted.left = 0;
    bool resource = false;
    try {
        static_cast<void>(input.bind(pin, raw, exhausted));
    } catch (const std::exception& e) {
        resource = std::string_view(e.what()).starts_with("resource:");
    }
    EXPECT_TRUE(resource);  // Resource failure is explicit and earns no rejected-mutant credit.
}

TEST(MermaidEr107SourcePreflight, CanonicalEncodingRejectsDuplicateKeysRealsAndUnsignedWrap) {
    const auto good = json::parse(R"({"z":null,"a":"ok","n":42})");
    ASSERT_TRUE(good);
    er::OracleWork baseline;
    EXPECT_EQ(er::detail::canonical(good->root(), baseline), R"({"a":"ok","n":42,"z":null})");
    const auto unicode = json::parse(R"("\u4e2d")");
    ASSERT_TRUE(unicode);
    er::OracleWork utf8;
    EXPECT_EQ(er::detail::canonical(unicode->root(), utf8), std::string("\"\xe4\xb8\xad\""));
    for (const auto source :
         {R"({"a":1,"a":2})", R"({"a":1.0})", R"({"a":18446744073709551615})"}) {
        const auto mutant = json::parse(source);
        ASSERT_TRUE(mutant);
        rejects_without_resource([&] {
            er::OracleWork w;
            static_cast<void>(er::detail::canonical(mutant->root(), w));
        });
    }
    // Oversized bytes are rejected before hashing/JSON parsing, never counted
    // as a semantic negative. No production Work or size limit is changed.
    er::OracleWork work;
    bool resource = false;
    try {
        er::verify_asset_bytes(std::string(er::kMaxFactBytes + 1, ' '), "", "", work);
    } catch (const std::exception& e) {
        resource = std::string_view(e.what()).starts_with("resource:");
    }
    EXPECT_TRUE(resource);
}

TEST(MermaidEr107SourcePreflight, RawChompingAndGenericTypesRemainSourceFacts) {
    Inputs input;
    std::size_t folded = 0, plain = 0, generic_types = 0, empty_roles = 0, one_space_roles = 0,
                two_space_roles = 0;
    for (const auto& pin : er::kPins) {
        const auto raw = input.raw(pin);
        er::OracleWork work;
        const auto facts = input.bind(pin, raw, work);
        const auto description = facts.source_model.get("metadata").get("description"),
                   style = description.get("yaml_style");
        const auto witness = facts.source_witnesses.at(
            static_cast<std::size_t>(description.get_int("value_witness")));
        if (style.is_str()) {
            EXPECT_EQ(style.as_str(), ">");
            EXPECT_TRUE(witness.get_string("source_lexeme").starts_with(">-\n"));
            ++folded;
        } else {
            EXPECT_TRUE(style.is_null());
            ++plain;
        }
        const auto entities = facts.source_model.get("entities");
        for (std::size_t i = 0; i < entities.size(); ++i) {
            const auto attrs = entities.at(i).get("attributes");
            for (std::size_t j = 0; j < attrs.size(); ++j)
                if (attrs.at(j).get_string("type_literal") == "type~T~") {
                    ++generic_types;
                    const auto ref = attrs.at(j).get_int("type_witness");
                    EXPECT_EQ(facts.source_witnesses.at(static_cast<std::size_t>(ref))
                                  .get_string("source_lexeme"),
                              "type~T~");
                }
        }
        const auto relations = facts.source_model.get("relationships");
        for (std::size_t i = 0; i < relations.size(); ++i) {
            const auto label = relations.at(i).get_string("role_literal");
            empty_roles += label.empty();
            one_space_roles += label == " ";
            two_space_roles += label == "  ";
            EXPECT_TRUE(relations.at(i).get("terminal_role").is_null());
        }
        EXPECT_TRUE(facts.source_model.get("metadata").get("effective_config").is_null());
    }
    EXPECT_EQ(folded, 103u);
    EXPECT_EQ(plain, 4u);
    EXPECT_EQ(generic_types, 10u);
    EXPECT_EQ(empty_roles, 1u);
    EXPECT_EQ(one_space_roles, 1u);
    EXPECT_EQ(two_space_roles, 1u);
    // B2 must separately account for upstream parseGenericTypes and markdown
    // display. This B1 source binder deliberately does not invent that Model.
}
