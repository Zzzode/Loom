/// RFC 0026 native policy and file-configuration contracts.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"
#include <gtest/gtest.h>
import std;
import loom.services.compact.model_context;
import loom.types.tool_types;
import loom.config.config;
import loom.serdes.json;
import loom.platform.env.env;

namespace {
using namespace loom::core;
using loom::services::compact::resolve_model_context;
namespace fs = std::filesystem;

ModelContextLimit profile(std::string model, std::uint32_t window,
                          std::optional<std::uint32_t> output = {}) {
    return {.model=std::move(model), .context_window_tokens=window, .max_output_tokens=output};
}

bool ancestry_clean(fs::path path) {
    std::error_code ec;
    for (;;) {
        if (fs::exists(path/".git",ec)) return false;
        if (path.parent_path()==path || path.parent_path().empty()) return true;
        path=path.parent_path();
    }
}

class ModelContextFiles : public testing::Test {
protected:
    void SetUp() override {
        std::optional<fs::path> base;
        for (const auto variable : {"XDG_RUNTIME_DIR","TMPDIR"}) {
            const auto value=loom::utils::env::get_env(variable);
            if (value && fs::is_directory(*value) && ancestry_clean(*value)) { base=*value; break; }
        }
        if (!base && fs::is_directory("/dev/shm") && ancestry_clean("/dev/shm")) base="/dev/shm";
        if (!base && ancestry_clean(fs::temp_directory_path())) base=fs::temp_directory_path();
        if (!base) GTEST_SKIP()<<"No Git-ancestry-clean temporary directory is available";
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for (std::uint64_t attempt=0;;++attempt) {
            root=*base/std::format("loom-model-context-{}-{}",stamp,attempt);
            if (fs::create_directory(root)) break;
        }
        previous_cwd=fs::current_path();
        fs::current_path(root);
        previous_model=loom::utils::env::get_env("LOOM_MODEL");
        previous_output=loom::utils::env::get_env("LOOM_MAX_TOKENS");
        (void)loom::utils::env::unset_env("LOOM_MODEL");
        (void)loom::utils::env::unset_env("LOOM_MAX_TOKENS");
        environment_changed=true;
        user=root/"user/settings.json";
        project=root/"project/settings.json";
        local=root/"project/settings.local.json";
    }
    void TearDown() override {
        if (environment_changed) {
            if (previous_model) (void)loom::utils::env::set_env("LOOM_MODEL",*previous_model);
            else (void)loom::utils::env::unset_env("LOOM_MODEL");
            if (previous_output) (void)loom::utils::env::set_env("LOOM_MAX_TOKENS",*previous_output);
            else (void)loom::utils::env::unset_env("LOOM_MAX_TOKENS");
        }
        if (previous_cwd) fs::current_path(*previous_cwd);
        if (!root.empty()) fs::remove_all(root);
    }
    ConfigManager manager() const { return ConfigManager(user,project,local); }
    static void write(const fs::path& path,std::string_view content) {
        fs::create_directories(path.parent_path());
        std::ofstream output(path);
        output<<content;
        EXPECT_TRUE(output.good());
    }
    static std::string read(const fs::path& path) {
        std::ifstream input(path);
        return std::string(std::istreambuf_iterator<char>(input),{});
    }
    std::string profile_json(const fs::path& path) const {
        const auto parsed=loom::utils::json::parse_file(path);
        return parsed ? parsed->root().get("modelContextLimits").to_string() : "";
    }
    fs::path root,user,project,local;
    std::optional<fs::path> previous_cwd;
    std::optional<std::string> previous_model,previous_output;
    bool environment_changed=false;
};
}

TEST(ModelContext, ExactScopedAndGenericProfilesPrecedeCatalog) {
    ModelContextConfig config{.fallback_window_tokens=123456};
    config.limits.push_back(profile("gpt-4.1",256000,8192));
    auto scoped=profile("gpt-4.1",1000000,32768);
    scoped.base_url="https://api.openai.com/v1///";
    scoped.wire_api="openai";
    config.limits.push_back(scoped);
    const auto exact=resolve_model_context(config,"gpt-4.1","https://api.openai.com/v1/","openai",64000);
    EXPECT_EQ(exact.window_tokens,1000000u);
    EXPECT_EQ(exact.output_tokens,32768u);
    EXPECT_EQ(exact.source,"profile");
    EXPECT_EQ(exact.output_cap_source,"profile");
    const auto other=resolve_model_context(config,"gpt-4.1","https://other.example/v1","openai",64000);
    EXPECT_EQ(other.window_tokens,256000u);
    EXPECT_EQ(other.output_tokens,8192u);
}

TEST(ModelContext, ContextOnlyProfilesRetainIndependentCatalogOutputCap) {
    ModelContextConfig config;
    config.limits.push_back(profile("gpt-4.1",256000));
    const auto resolved=resolve_model_context(config,"gpt-4.1","https://api.openai.com/v1","openai",64000);
    EXPECT_EQ(resolved.window_tokens,256000u);
    EXPECT_EQ(resolved.source,"profile");
    EXPECT_EQ(resolved.output_cap,std::optional<std::uint32_t>{32768});
    EXPECT_EQ(resolved.output_cap_source,"catalog");
    EXPECT_EQ(resolved.output_tokens,32768u);
    const auto proxy=resolve_model_context(config,"gpt-4.1","https://proxy.example/v1","openai",64000);
    EXPECT_EQ(proxy.output_tokens,64000u);
    EXPECT_FALSE(proxy.output_cap);
    EXPECT_EQ(proxy.output_cap_source,"none");
}

TEST(ModelContext, ScopedContextCanInheritGenericOutputCap) {
    ModelContextConfig config;
    config.limits.push_back(profile("gpt-4.1",1000000,4096));
    auto scoped=profile("gpt-4.1",256000);
    scoped.base_url="https://api.openai.com/v1";
    scoped.wire_api="openai";
    config.limits.push_back(scoped);
    const auto resolved=resolve_model_context(config,"gpt-4.1",*scoped.base_url,"openai",64000);
    EXPECT_EQ(resolved.window_tokens,256000u);
    EXPECT_EQ(resolved.output_tokens,4096u);
    EXPECT_EQ(resolved.output_cap_source,"profile");
}

TEST(ModelContext, OfficialCatalogUsesExactEndpointWireAndModel) {
    ModelContextConfig config{.fallback_window_tokens=98765};
    for (const auto model : {"gpt-4.1","gpt-4.1-2025-04-14"}) {
        const auto resolved=resolve_model_context(config,model,"https://api.openai.com/v1/","openai",90000);
        EXPECT_EQ(resolved.window_tokens,1047576u);
        EXPECT_EQ(resolved.output_tokens,32768u);
        EXPECT_EQ(resolved.source,"catalog");
        EXPECT_EQ(resolved.output_cap_source,"catalog");
    }
    for (const auto model : {"claude-haiku-4-5-20251001","claude-sonnet-4-5-20250929","claude-opus-4-5-20251101"}) {
        const auto resolved=resolve_model_context(config,model,"https://api.anthropic.com","messages",90000);
        EXPECT_EQ(resolved.window_tokens,200000u);
        EXPECT_EQ(resolved.output_tokens,64000u);
        EXPECT_EQ(resolved.source,"catalog");
    }
    for (const auto endpoint : {"https://proxy.example/v1","https://api.openai.com.evil/v1",
                               "https://api.openai.com/tenant/v1","https://api.openai.com/v1?key=x"}) {
        const auto resolved=resolve_model_context(config,"gpt-4.1",endpoint,"openai",90000);
        EXPECT_EQ(resolved.window_tokens,98765u);
        EXPECT_EQ(resolved.output_tokens,90000u);
        EXPECT_EQ(resolved.source,"fallback");
        EXPECT_FALSE(resolved.output_cap);
    }
    EXPECT_EQ(resolve_model_context(config,"gpt-4.1","https://api.openai.com","messages",1000).source,"fallback");
    EXPECT_EQ(resolve_model_context(config,"claude-sonnet-4-5-20250929","https://api.anthropic.com","openai",1000).source,"fallback");
}

TEST(ModelContext, SimilarNamesAndUnverifiedAliasesDoNotGuessCapacities) {
    const ModelContextConfig config{.fallback_window_tokens=135791};
    for (const auto model : {"my-gpt-4.1","gpt-4.1-custom","gpt-4.1-mini","GPT-4.1","gpt-4.1-2025-04-15","gpt-4.1[1m]"})
        EXPECT_EQ(resolve_model_context(config,model,"https://api.openai.com/v1","openai",70000).window_tokens,135791u);
    for (const auto model : {"claude-sonnet-4-5","claude-sonnet-4-5-20250929[1m]","prefix-claude-haiku-4-5-20251001"})
        EXPECT_EQ(resolve_model_context(config,model,"https://api.anthropic.com","messages",70000).source,"fallback");
}

TEST(ModelContext, SwitchingOutputCapsKeepsOriginalRequestedOutputReversible) {
    ModelContextConfig config{.fallback_window_tokens=234567};
    config.limits={profile("large",1000000,100000),profile("small",256000,8192)};
    const auto large=resolve_model_context(config,"large","http://localhost:1","openai",90000);
    const auto small=resolve_model_context(config,"small","http://localhost:1","openai",90000);
    const auto back=resolve_model_context(config,"large","http://localhost:1","openai",90000);
    EXPECT_EQ(large.output_tokens,90000u);
    EXPECT_EQ(small.output_tokens,8192u);
    EXPECT_EQ(back.output_tokens,90000u);
    const auto unknown=resolve_model_context(config,"unknown-child","http://localhost:1","openai",90000);
    EXPECT_EQ(unknown.window_tokens,234567u);
    EXPECT_EQ(unknown.output_tokens,90000u);
    EXPECT_EQ(unknown.source,"fallback");
}

TEST(ModelContext, ContextBudgetDoesNotSilentlyTruncateRequestedOutput) {
    ModelContextConfig config;
    config.limits={profile("tiny",1000)};
    const auto resolved=resolve_model_context(config,"tiny","http://localhost:1","messages",2000);
    EXPECT_EQ(resolved.window_tokens,1000u);
    EXPECT_EQ(resolved.output_tokens,2000u);
    EXPECT_FALSE(resolved.output_cap);
}

TEST(ModelContext, ScopedIdentityPreservesTenantPaths) {
    ModelContextConfig config;
    auto scoped=profile("model",256000);
    scoped.base_url="https://gateway.example/tenant///";
    scoped.wire_api="openai";
    config.limits={scoped};
    EXPECT_EQ(resolve_model_context(config,"model","https://gateway.example/tenant","openai",1024).source,"profile");
    EXPECT_EQ(resolve_model_context(config,"model","https://gateway.example/tenant/v1","openai",1024).source,"fallback");
    EXPECT_EQ(resolve_model_context(config,"model","https://gateway.example/tenant","messages",1024).source,"fallback");
}

TEST_F(ModelContextFiles, LegacyScalarsRemainFallbackAndProfilesAreFileOnly) {
    write(user,R"({"model":{"default_model":"legacy","context_window_size":345678,"max_output_tokens":8192},"contextWindowSize":456789,"maxOutputTokens":4096})");
    auto config=manager();
    ASSERT_TRUE(config.load());
    EXPECT_EQ(config.settings().model.context_window_size,456789u);
    EXPECT_EQ(config.settings().model.max_output_tokens,4096u);
    EXPECT_TRUE(config.settings().model.context_limits.empty());
    const ModelContextConfig policy{.fallback_window_tokens=config.settings().model.context_window_size};
    EXPECT_EQ(resolve_model_context(policy,"gpt-4.1","https://api.openai.com/v1","openai",4096).window_tokens,1047576u);
    EXPECT_EQ(resolve_model_context(policy,"private-model","https://gateway.example","openai",4096).window_tokens,456789u);
    const auto json=loom::utils::json::parse("[]");
    ASSERT_TRUE(json);
    const auto rejected=config.set_user_setting("modelContextLimits",json->root());
    EXPECT_FALSE(rejected);
}

TEST_F(ModelContextFiles, ParsesGenericScopedAndIntegralJsonNumbers) {
    write(project,R"({"modelContextLimits":[{"model":"Exact-Model","contextWindowSize":1000000.0,"maxOutputTokens":65536.0},{"model":"Exact-Model","contextWindowSize":256000,"baseUrl":"https://gateway.example/tenant///","wireApi":"openai"}]})");
    auto config=manager();
    const auto loaded=config.load();
    ASSERT_TRUE(loaded)<<loaded.error().format();
    const auto& limits=config.settings().model.context_limits;
    ASSERT_EQ(limits.size(),2u);
    EXPECT_EQ(limits[0].model,"Exact-Model");
    EXPECT_EQ(limits[0].context_window_tokens,1000000u);
    EXPECT_EQ(limits[0].max_output_tokens,std::optional<std::uint32_t>{65536});
    EXPECT_EQ(limits[1].base_url,std::optional<std::string>{"https://gateway.example/tenant"});
    EXPECT_FALSE(limits[1].max_output_tokens);
}

TEST_F(ModelContextFiles, RejectsInvalidProfilesAtEveryJsonTier) {
    const std::vector<std::string> invalid{
        "null","{}","[null]","[{}]",
        R"([{"model":"","contextWindowSize":1000}])",
        R"([{"model":"   ","contextWindowSize":1000}])",
        R"([{"model":"x","contextWindowSize":0}])",
        R"([{"model":"x","contextWindowSize":-1}])",
        R"([{"model":"x","contextWindowSize":1.5}])",
        R"([{"model":"x","contextWindowSize":4294967296}])",
        R"([{"model":"x","contextWindowSize":"1000000"}])",
        R"([{"model":"x","contextWindowSize":1000,"maxOutputTokens":0}])",
        R"([{"model":"x","contextWindowSize":1000,"maxOutputTokens":1.5}])",
        R"([{"model":"x","contextWindowSize":1000,"maxOutputTokens":4294967296}])",
        R"([{"model":"x","contextWindowSize":1000,"baseUrl":"https://gateway.example"}])",
        R"([{"model":"x","contextWindowSize":1000,"wireApi":"openai"}])",
        R"([{"model":"x","contextWindowSize":1000,"baseUrl":"https://gateway.example","wireApi":"unknown"}])",
        R"([{"model":"x","contextWindowSize":1000,"baseUrl":"https://user:secret@gateway.example","wireApi":"openai"}])",
        R"([{"model":"x","contextWindowSize":1000,"baseUrl":"https://gateway.example?api_key=secret","wireApi":"openai"}])",
        R"([{"model":"x","contextWindowSize":1000},{"model":"x","contextWindowSize":2000}])",
        R"([{"model":"x","contextWindowSize":1000,"baseUrl":"https://gateway.example/","wireApi":"openai"},{"model":"x","contextWindowSize":2000,"baseUrl":"https://gateway.example","wireApi":"openai"}])"};
    for (const auto& path : {user,project,local}) {
        for (const auto& value : invalid) {
            SCOPED_TRACE(path.filename().string());
            SCOPED_TRACE(value);
            for (const auto& file : {user,project,local}) fs::remove(file);
            write(path,"{\"modelContextLimits\":"+value+"}");
            auto config=manager();
            const auto result=config.load(LoadOptions{.quiet=true});
            ASSERT_FALSE(result);
            EXPECT_NE(result.error().message.find("modelContextLimits"),std::string::npos);
            EXPECT_EQ(result.error().message.find("secret"),std::string::npos);
        }
    }
}

TEST_F(ModelContextFiles, HigherTierOverlaysIdentityAndEmptyArrayClearsPriorProfiles) {
    write(user,R"({"modelContextLimits":[{"model":"same","contextWindowSize":1000000},{"model":"retained","contextWindowSize":512000}]})");
    write(project,R"({"modelContextLimits":[{"model":"same","contextWindowSize":256000}]})");
    write(local,R"({"modelContextLimits":[{"model":"same","contextWindowSize":900000,"baseUrl":"https://gateway.example","wireApi":"openai"}]})");
    auto config=manager();
    ASSERT_TRUE(config.load());
    const auto policy=ModelContextConfig{.limits=config.settings().model.context_limits};
    ASSERT_EQ(policy.limits.size(),3u);
    EXPECT_EQ(resolve_model_context(policy,"same","https://gateway.example","openai",1000).window_tokens,900000u);
    EXPECT_EQ(resolve_model_context(policy,"same","https://other.example","openai",1000).window_tokens,256000u);
    EXPECT_EQ(resolve_model_context(policy,"retained","https://other.example","openai",1000).window_tokens,512000u);
    write(project,R"({"modelContextLimits":[]})");
    fs::remove(local);
    ASSERT_TRUE(config.load());
    EXPECT_TRUE(config.settings().model.context_limits.empty());
}

TEST_F(ModelContextFiles, InvalidArrayDoesNotPartiallyReplaceValidLowerTierProfiles) {
    write(user,R"({"modelContextLimits":[{"model":"same","contextWindowSize":1000000}]})");
    write(project,R"({"modelContextLimits":[{"model":"same","contextWindowSize":256000},{"model":"invalid","contextWindowSize":0}]})");
    auto config=manager();
    EXPECT_FALSE(config.load());
    ASSERT_EQ(config.settings().model.context_limits.size(),1u);
    EXPECT_EQ(config.settings().model.context_limits.front().context_window_tokens,1000000u);
}

TEST_F(ModelContextFiles, SavePreservesDestinationProfilesWithoutCopyingOtherTiers) {
    write(user,R"({"modelContextLimits":[{"model":"user-private","contextWindowSize":1000000,"baseUrl":"https://user.example","wireApi":"openai"}]})");
    write(project,R"({"modelContextLimits":[{"model":"project-model","contextWindowSize":256000,"futureMetadata":{"keep":true}}],"unknownSibling":{"keep":47}})");
    write(local,R"({"modelContextLimits":[{"model":"local-private","contextWindowSize":512000}]})");
    const auto user_before=read(user),local_before=read(local),profiles_before=profile_json(project);
    auto config=manager();
    ASSERT_TRUE(config.load());
    ASSERT_EQ(config.settings().model.context_limits.size(),3u);
    ASSERT_TRUE(config.save());
    EXPECT_EQ(profile_json(project),profiles_before);
    EXPECT_EQ(read(user),user_before);
    EXPECT_EQ(read(local),local_before);
    EXPECT_EQ(read(project).find("user-private"),std::string::npos);
    EXPECT_EQ(read(project).find("local-private"),std::string::npos);
    const auto parsed=loom::utils::json::parse_file(project);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->root().get("unknownSibling").get("keep").as_int(),47);
    auto restored=manager();
    ASSERT_TRUE(restored.load());
    EXPECT_EQ(restored.settings().model.context_limits.size(),3u);
}

TEST_F(ModelContextFiles, SaveDoesNotCreateAProfileKeyFromMergedPolicy) {
    write(user,R"({"modelContextLimits":[{"model":"user-model","contextWindowSize":1000000}]})");
    write(project,R"({"unknownSibling":true})");
    auto config=manager();
    ASSERT_TRUE(config.load());
    ASSERT_TRUE(config.save());
    const auto parsed=loom::utils::json::parse_file(project);
    ASSERT_TRUE(parsed);
    EXPECT_FALSE(parsed->root().get("modelContextLimits").valid());
}

TEST_F(ModelContextFiles, ScalarPatchesPreserveProfileJsonAndDoNotExpandTheSettingSurface) {
    write(user,R"({"modelContextLimits":[{"model":"exact","contextWindowSize":1000000,"futureMetadata":[1,2,3]}],"unknownSibling":true})");
    const auto before=profile_json(user);
    auto config=manager();
    ASSERT_TRUE(config.load());
    const auto value=loom::utils::json::parse("7");
    ASSERT_TRUE(value);
    ASSERT_TRUE(config.set_user_setting("maxRetries",value->root()));
    EXPECT_EQ(profile_json(user),before);
    const auto unchanged=read(user);
    const auto array=loom::utils::json::parse("[]");
    ASSERT_TRUE(array);
    EXPECT_FALSE(config.set_user_setting("modelContextLimits",array->root()));
    EXPECT_EQ(read(user),unchanged);
}


TEST(ModelContext, AutomaticOutputSeparatesProviderDefaultsFromContextReserve) {
    ModelContextConfig config;
    config.limits = {profile("large", 1000000, 64000), profile("small", 32768, 8192)};
    for (const auto wire : {"messages", "openai"}) {
        const auto large = resolve_model_context(config, "large", "https://gateway.example", wire, 0);
        const auto small = resolve_model_context(config, "small", "https://gateway.example", wire, 0);
        const auto back = resolve_model_context(config, "large", "https://gateway.example", wire, 0);
        EXPECT_EQ(large.output_tokens, 64000u);
        EXPECT_EQ(small.output_tokens, 8192u);
        EXPECT_EQ(back.output_tokens, large.output_tokens);
        EXPECT_EQ(large.request_output_tokens.has_value(), std::string_view(wire) == "messages");
        if (large.request_output_tokens) EXPECT_EQ(*large.request_output_tokens, large.output_tokens);
        const auto explicit_limit = resolve_model_context(config, "small", "https://gateway.example", wire, 1024);
        EXPECT_EQ(explicit_limit.request_output_tokens, std::optional<std::uint32_t>{1024});
    }
}

TEST(ModelContext, AutomaticUnknownAndTinyWindowsRetainInputHeadroomWithoutUnderflow) {
    for (const auto window : {0u, 1u, 64u, 65u, 100u, 1000u, 200000u}) {
        const ModelContextConfig config{.fallback_window_tokens = window};
        const auto result = resolve_model_context(config, "private", "https://gateway.example", "messages", 0);
        const auto safety = std::max<std::uint64_t>(64, window / 50);
        const auto expected = std::max<std::uint64_t>(1, std::min<std::uint64_t>(16384,
            window > safety ? (window - safety) / 2 : 0));
        EXPECT_EQ(result.output_tokens, expected);
        EXPECT_EQ(result.request_output_tokens, std::optional<std::uint32_t>{static_cast<std::uint32_t>(expected)});
        EXPECT_EQ(result.output_cap_source, "none");
    }
}

TEST(ModelContext, ManualThinkingRaisesOnlyAutomaticTargetWithinCapacityAndHeadroom) {
    ModelContextConfig config;
    config.limits = {profile("capped", 200000, 8192), profile("tiny", 1000)};
    EXPECT_EQ(resolve_model_context(config, "unknown", "https://gateway.example", "messages", 0, 32769).output_tokens, 32769u);
    EXPECT_EQ(resolve_model_context(config, "unknown", "https://gateway.example", "messages", 4096, 32769).output_tokens, 4096u);
    EXPECT_EQ(resolve_model_context(config, "capped", "https://gateway.example", "messages", 0, 32769).output_tokens, 8192u);
    EXPECT_EQ(resolve_model_context(config, "tiny", "https://gateway.example", "messages", 0, 8193).output_tokens, 468u);
    EXPECT_EQ(resolve_model_context(config, "unknown", "https://gateway.example", "messages", 0,
        std::uint64_t{std::numeric_limits<std::uint32_t>::max()} + 1).output_tokens, 98000u);
}

TEST(ModelMetadata, ParsesExactScopedCapacitiesWithIndependentOptionalOutput) {
    const auto limits = loom::services::compact::parse_model_metadata(R"({"data":[
      {"id":"opensource/exact-model","context_length":1048576,"max_output_tokens":65536,"unknown":true},
      {"id":"context-only","context_length":256000},
      {"id":"bad-output","context_length":128000,"max_output_tokens":"4096"}
    ]})", "https://gateway.example/tenant/v1/", "messages");
    ASSERT_EQ(limits.size(), 3u);
    EXPECT_EQ(limits[0].model, "opensource/exact-model");
    EXPECT_EQ(limits[0].context_window_tokens, 1048576u);
    EXPECT_EQ(limits[0].max_output_tokens, std::optional<std::uint32_t>{65536});
    EXPECT_FALSE(limits[1].max_output_tokens);
    EXPECT_FALSE(limits[2].max_output_tokens);
    for (const auto& limit : limits) {
        EXPECT_EQ(limit.base_url, std::optional<std::string>{"https://gateway.example/tenant/v1/"});
        EXPECT_EQ(limit.wire_api, std::optional<std::string>{"messages"});
    }
}

TEST(ModelMetadata, RejectsInvalidCapacityShapesAndConflictingDuplicateIds) {
    using loom::services::compact::parse_model_metadata;
    for (const auto body : {"not-json", "[]", "{}", R"({"data":{}})",
         R"({"data":[{"id":"usage-only","usage":{"input_tokens":1048576}}]})"})
        EXPECT_TRUE(parse_model_metadata(body, "https://gateway.example", "openai").empty()) << body;
    const auto limits = parse_model_metadata(R"({"data":[
      {"id":"","context_length":100}, {"context_length":100},
      {"id":3,"context_length":100}, {"id":"zero","context_length":0},
      {"id":"negative","context_length":-1}, {"id":"overflow","context_length":4294967296},
      {"id":"float","context_length":1048576.0}, {"id":"fraction","context_length":1.5},
      {"id":"string","context_length":"1048576"}, {"id":"bool","context_length":true},
      {"id":"null","context_length":null},
      {"id":"conflict","context_length":1000}, {"id":"conflict","context_length":2000},
      {"id":"conflict","context_length":1000},
      {"id":"output-conflict","context_length":1000,"max_output_tokens":100},
      {"id":"output-conflict","context_length":1000,"max_output_tokens":200},
      {"id":"same","context_length":3000}, {"id":"same","context_length":3000},
      {"id":"max-uint","context_length":4294967295}
    ]})", "https://gateway.example", "openai");
    ASSERT_EQ(limits.size(), 2u);
    const auto same = std::ranges::find(limits, "same", &ModelContextLimit::model);
    ASSERT_NE(same, limits.end());
    EXPECT_EQ(same->context_window_tokens, 3000u);
    const auto maximum = std::ranges::find(limits, "max-uint", &ModelContextLimit::model);
    ASSERT_NE(maximum, limits.end());
    EXPECT_EQ(maximum->context_window_tokens, std::numeric_limits<std::uint32_t>::max());
    for (auto invalid : {"0", "-1", "4294967296", "4096.0", "true", "null", "\"4096\""}) {
        const auto parsed = parse_model_metadata(std::format(
            R"({{"data":[{{"id":"context-kept","context_length":10000,"max_output_tokens":{}}}]}})", invalid),
            "https://gateway.example", "openai");
        ASSERT_EQ(parsed.size(), 1u) << invalid;
        EXPECT_FALSE(parsed[0].max_output_tokens) << invalid;
    }
}

TEST(ModelMetadata, PolicyPrecedenceAndEndpointIdentityRemainIndependentForContextAndOutput) {
    ModelContextConfig config{.fallback_window_tokens = 123456};
    config.metadata_limits = loom::services::compact::parse_model_metadata(
        R"({"data":[{"id":"gpt-4.1","context_length":900000,"max_output_tokens":12000},{"id":"other","context_length":500000}]})",
        "https://api.openai.com/v1", "openai");
    auto resolved = resolve_model_context(config, "gpt-4.1", "https://api.openai.com/v1", "openai", 64000);
    EXPECT_EQ(resolved.window_tokens, 900000u);
    EXPECT_EQ(resolved.source, "metadata");
    EXPECT_EQ(resolved.output_cap, std::optional<std::uint32_t>{12000});
    EXPECT_EQ(resolved.output_cap_source, "metadata");
    config.metadata_limits[0].max_output_tokens.reset();
    resolved = resolve_model_context(config, "gpt-4.1", "https://api.openai.com/v1", "openai", 64000);
    EXPECT_EQ(resolved.window_tokens, 900000u);
    EXPECT_EQ(resolved.output_tokens, 32768u);
    EXPECT_EQ(resolved.output_cap_source, "catalog");
    config.limits = {profile("gpt-4.1", 256000)};
    config.metadata_limits[0].max_output_tokens = 12000;
    resolved = resolve_model_context(config, "gpt-4.1", "https://api.openai.com/v1", "openai", 64000);
    EXPECT_EQ(resolved.window_tokens, 256000u);
    EXPECT_EQ(resolved.source, "profile");
    EXPECT_EQ(resolved.output_tokens, 12000u);
    auto scoped = profile("gpt-4.1", 128000, 4096);
    scoped.base_url = "https://api.openai.com/v1";
    scoped.wire_api = "openai";
    config.limits.push_back(scoped);
    resolved = resolve_model_context(config, "gpt-4.1", "https://api.openai.com/v1", "openai", 64000);
    EXPECT_EQ(resolved.window_tokens, 128000u);
    EXPECT_EQ(resolved.output_tokens, 4096u);
    EXPECT_EQ(resolved.output_cap_source, "profile");
    for (auto endpoint : {"https://api.openai.com/tenant/v1", "https://different.example/v1"}) {
        const auto isolated = resolve_model_context(config, "other", endpoint, "openai", 0);
        EXPECT_EQ(isolated.window_tokens, 123456u);
        EXPECT_EQ(isolated.source, "fallback");
    }
    EXPECT_EQ(resolve_model_context(config, "other", "https://api.openai.com/v1", "messages", 0).source, "fallback");
    EXPECT_EQ(resolve_model_context(config, "not-other", "https://api.openai.com/v1", "openai", 0).source, "fallback");
}
