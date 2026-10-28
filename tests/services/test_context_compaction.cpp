/// RFC 0025 policy invariants, independent of wire and execution owners.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"
#include <gtest/gtest.h>
import std;
import loom.services.compact.context_compaction;
import loom.types.types;

namespace {
using namespace loom::core;
namespace cc = loom::services::compact;

std::string checkpoint(std::string_view detail = "Keep the current task.") {
    return "## Goal\nComplete the task.\n## Constraints\n" + std::string(detail) +
           "\n## Decisions\nUse the existing design.\n## Progress\nInspected the inputs.\n"
           "## Next Steps\nContinue verification.\n## References\n/work/source.cpp\n";
}

Message user(std::string text) {
    UserMessage message{};
    message.content.emplace_back(TextBlock{std::move(text)});
    return message;
}

Message assistant(std::string text) {
    AssistantMessage message{};
    message.content.emplace_back(TextBlock{std::move(text)});
    return message;
}

std::vector<Message> history(std::size_t bytes = 14000) {
    return {user(std::string(bytes, 'x') + " NEVER COMMIT; result=47"),
            assistant(std::string(bytes, 'y')), user("Continue the same task verbatim.")};
}

cc::SummaryResponse complete(std::string text = checkpoint()) {
    return {.text = std::move(text), .complete = true};
}

cc::ContextPolicy policy() {
    return {.window_tokens = 4000, .output_tokens = 128, .threshold = 0.9};
}
}  // namespace

TEST(ContextCompaction, AccountsForEveryBlockAndSchema) {
    const auto baseline = cc::estimate_message_tokens(user(""));
    std::vector<ContentBlock> blocks{
        TextBlock{std::string(1000, 't')},
        ThinkingBlock{std::string(1000, 'r'), "signature"},
        ToolUseBlock{ToolUseId{"call"}, "Read", "{\"path\":\"" + std::string(1000, 'p') + "\"}"},
        ToolResultBlock{ToolUseId{"call"}, std::string(1000, 'o'), false},
        ToolResultBlock{
            ToolUseId{"mixed"},
            std::vector<ToolResultContentItem>{
                {.type = "text", .text = std::string(1000, 'm')},
                {.type = "image", .media_type = "image/png", .data = std::string(2048, 'a')}},
            true},
        ImageBlock{.media_type = "image/png",
                   .data = std::string(2048, 'a'),
                   .width = 1024,
                   .height = 1024},
        DocumentBlock{"application/pdf", std::string(2048, 'b')}};
    for (const auto& block : blocks) {
        UserMessage message{};
        message.content.push_back(block);
        EXPECT_GT(cc::estimate_message_tokens(message), baseline + 50);
    }
    ToolUseMessage legacy_call{};
    legacy_call.tool_name = "Read";
    legacy_call.tool_input_json = "{\"path\":\"" + std::string(2000, 'p') + "\"}";
    EXPECT_GT(cc::estimate_message_tokens(legacy_call), baseline + 100);
    ToolResultMessage ordinary{};
    ordinary.tool_use_id = ToolUseId{"actual"};
    ordinary.content.emplace_back(TextBlock{std::string(20000, 't')});
    EXPECT_GT(cc::estimate_message_tokens(ordinary), 1000u);
    std::vector<Message> messages{user("hello")};
    const auto plain = cc::estimate_request_tokens({.messages = messages});
    EXPECT_GT(cc::estimate_request_tokens(
                  {.messages = messages, .system_prompt = std::string(1000, 's')}),
              plain + 100);
    EXPECT_GT(
        cc::estimate_request_tokens({.messages = messages, .tool_schema = std::string(1000, 'j')}),
        plain + 100);
}

TEST(ContextCompaction, ReservesOutputAndCalibratesUsage) {
    EXPECT_EQ(
        cc::usable_input_tokens({.window_tokens = 1000, .output_tokens = 200, .threshold = 0.9}),
        736u);
    EXPECT_EQ(
        cc::usable_input_tokens({.window_tokens = 1000, .output_tokens = 100, .threshold = 0.5}),
        500u);
    std::vector<Message> messages{user(std::string(800, 'x'))};
    cc::RequestContext context{.messages = messages};
    const auto local = cc::estimate_request_tokens(context, false);
    context.observed = {.input_tokens = 900, .estimated_tokens = local - 20};
    EXPECT_EQ(cc::estimate_request_tokens(context), 920u);
    context.observed = {.input_tokens = 1, .estimated_tokens = local + 20};
    EXPECT_EQ(cc::estimate_request_tokens(context), local);
}

TEST(ContextCompaction, RetainsRequestAndClosedToolTail) {
    auto messages = history();
    messages.erase(messages.begin() + 2);
    messages.insert(messages.begin(), user("single human task: NEVER COMMIT"));
    for (int round = 0; round < 10; ++round) {
        AssistantMessage calls{};
        calls.content.emplace_back(
            ToolUseBlock{ToolUseId{"a" + std::to_string(round)}, "Read", "{}"});
        calls.content.emplace_back(
            ToolUseBlock{ToolUseId{"b" + std::to_string(round)}, "Read", "{}"});
        messages.emplace_back(calls);
        for (char prefix : {'a', 'b'}) {
            ToolResultMessage result{};
            result.tool_use_id = ToolUseId{std::string(1, prefix) + std::to_string(round)};
            result.content.emplace_back(TextBlock{std::string(500, prefix)});
            messages.emplace_back(result);
        }
    }
    auto result = cc::compact_context(
        {.messages = messages, .request_anchor = 0}, policy(),
        [](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
            return complete();
        });
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_TRUE(std::ranges::contains(result->retained_indices, 0u));
    EXPECT_TRUE(std::ranges::is_sorted(result->retained_indices));
    EXPECT_LT(result->after_tokens, result->before_tokens);
    EXPECT_LE(result->after_tokens, cc::usable_input_tokens(policy()));
    for (std::size_t i = 3; i < messages.size(); i += 3) {
        const bool kept = std::ranges::contains(result->retained_indices, i);
        EXPECT_EQ(kept, std::ranges::contains(result->retained_indices, i + 1));
        EXPECT_EQ(kept, std::ranges::contains(result->retained_indices, i + 2));
    }
}

TEST(ContextCompaction, RejectsDanglingParallelToolGroups) {
    auto messages = history();
    AssistantMessage pending{};
    pending.content.emplace_back(ToolUseBlock{ToolUseId{"pending"}, "Read", "{}"});
    messages.emplace_back(pending);
    int calls = 0;
    auto result = cc::compact_context(
        {.messages = messages, .request_anchor = 2}, policy(),
        [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
            ++calls;
            return complete();
        });
    EXPECT_FALSE(result);
    EXPECT_EQ(calls, 0);
}

TEST(ContextCompaction, PreservesLateConstraintAcrossChunks) {
    std::string long_text;
    for (int i = 0; i < 4000; ++i)
        long_text += "验证";
    long_text += "NEVER COMMIT; completed result=47; /work/exact.cpp";
    std::vector<Message> messages{user(long_text), assistant(std::string(6000, 'a')),
                                  user("finish now")};
    bool saw_late = false;
    std::size_t bytes = 0;
    auto result = cc::compact_context(
        {.messages = messages, .request_anchor = 2}, policy(),
        [&](const cc::SummaryRequest& request) -> loom::core::Result<cc::SummaryResponse> {
            EXPECT_NE(request.instructions.find("CONTEXT CHECKPOINT"), std::string::npos);
            EXPECT_GT(request.max_output_tokens, 0u);
            // Conversion to UTF-32 rejects a chunk boundary in the middle of a scalar.
            for (std::size_t i = 0; i < request.transcript.size();) {
                const auto c = static_cast<unsigned char>(request.transcript[i]);
                const std::size_t n = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
                EXPECT_LE(i + n, request.transcript.size());
                for (std::size_t j = 1; j < n && i + j < request.transcript.size(); ++j)
                    EXPECT_EQ(static_cast<unsigned char>(request.transcript[i + j]) & 0xc0, 0x80);
                i += n;
            }
            bytes += request.transcript.size();
            saw_late |= request.transcript.find("NEVER COMMIT") != std::string::npos;
            return complete(checkpoint(
                saw_late ? "NEVER COMMIT; completed result=47; /work/exact.cpp" : "Continue"));
        });
    ASSERT_TRUE(result) << result.error().message;
    EXPECT_TRUE(saw_late);
    EXPECT_GE(bytes, long_text.size());
    EXPECT_NE(result->summary.find("NEVER COMMIT"), std::string::npos);
    EXPECT_NE(result->summary.find("result=47"), std::string::npos);
    EXPECT_LE(result->summary_requests, 16u);
}

TEST(ContextCompaction, RejectsEmptyTruncatedExpandingAndOversizedSummary) {
    const auto messages = history();
    const std::vector<cc::SummaryResponse> invalid{
        {.text = "", .complete = true},
        {.text = checkpoint(), .complete = false},
        {.text = checkpoint(), .complete = true, .has_tool_use = true},
        {.text = "Unstructured ordinary assistant response", .complete = true},
        {.text = checkpoint(std::string(120000, 'x')), .complete = true},
        {.text = checkpoint(std::string(17000, 'x')), .complete = true}};
    for (const auto& response : invalid) {
        auto result = cc::compact_context(
            {.messages = messages, .request_anchor = 2}, policy(),
            [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
                return response;
            });
        EXPECT_FALSE(result) << response.text.substr(0, 60);
    }
}

TEST(ContextCompaction, BoundsSummaryOverflowRetries) {
    const auto messages = history(500000);
    int calls = 0;
    auto result = cc::compact_context(
        {.messages = messages, .request_anchor = 2}, policy(),
        [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
            ++calls;
            return std::unexpected(loom::core::Error::make(
                loom::core::ErrorCode::ContextWindowExceeded, "context_length_exceeded"));
        });
    EXPECT_FALSE(result);
    EXPECT_GE(calls, 1);
    EXPECT_LE(calls, 4);
    calls = 0;
    result = cc::compact_context(
        {.messages = messages, .request_anchor = 2}, policy(),
        [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
            ++calls;
            return complete();
        });
    EXPECT_LE(calls, 16);
    EXPECT_FALSE(result)
        << "A transcript needing more than 16 complete chunks must not be silently truncated";
}

TEST(ContextCompaction, HonoursCancellation) {
    const auto messages = history();
    int calls = 0;
    auto result = cc::compact_context(
        {.messages = messages, .request_anchor = 2}, policy(),
        [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
            ++calls;
            return complete();
        },
        [] { return true; });
    EXPECT_FALSE(result);
    EXPECT_EQ(calls, 0);
    result = cc::compact_context(
        {.messages = messages, .request_anchor = 2}, policy(),
        [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
            ++calls;
            return complete();
        },
        [&] { return calls > 0; });
    EXPECT_FALSE(result);
}

TEST(ContextCompaction, RejectsUncompressibleLatestRequestAndStaticOverhead) {
    const auto messages = history();
    int calls = 0;
    auto summarize = [&](const cc::SummaryRequest&) -> loom::core::Result<cc::SummaryResponse> {
        ++calls;
        return complete();
    };
    EXPECT_FALSE(
        cc::compact_context({.messages = messages, .request_anchor = 0}, policy(), summarize));
    EXPECT_FALSE(cc::compact_context(
        {.messages = messages, .system_prompt = std::string(50000, 's'), .request_anchor = 2},
        policy(), summarize));
    EXPECT_EQ(calls, 0);
}

TEST(ContextCompaction, ClassifiesStructuredAndCaseInsensitiveContextFailures) {
    for (std::string_view text : {"prompt is too long", "PROMPT_TOO_LONG",
                                  "context_length_exceeded", "maximum context length exceeded"})
        EXPECT_EQ(cc::classify_context_failure(400, text),
                  cc::ContextFailure::ContextTokensExceeded)
            << text;
    EXPECT_EQ(cc::classify_context_failure(413, "payload too large"),
              cc::ContextFailure::RequestBytesExceeded);
    EXPECT_EQ(cc::classify_context_failure(401, "invalid authentication"),
              cc::ContextFailure::None);
    EXPECT_EQ(cc::classify_context_failure(429, "quota exhausted"), cc::ContextFailure::None);
}

TEST(ContextCompaction, NeverRetriesAuthenticationQuotaOrOrdinaryTransportErrors) {
    const auto messages = history();
    for (const auto code :
         {ErrorCode::AuthenticationFailed, ErrorCode::RateLimited, ErrorCode::ConnectionFailed}) {
        int calls = 0;
        const auto result =
            cc::compact_context({.messages = messages, .request_anchor = 2}, policy(),
                                [&](const cc::SummaryRequest&) -> Result<cc::SummaryResponse> {
                                    ++calls;
                                    return std::unexpected(Error::make(code, "ordinary failure"));
                                });
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code, code);
        EXPECT_EQ(calls, 1);
    }
}

TEST(ContextCompaction, LatestRequestOnlyDoesNotReportFalseCompactionSuccess) {
    std::vector<Message> messages{user("CURRENT_REQUEST_EXACT")};
    int calls = 0;
    const auto result =
        cc::compact_context({.messages = messages, .request_anchor = 0}, policy(),
                            [&](const cc::SummaryRequest&) -> Result<cc::SummaryResponse> {
                                ++calls;
                                return complete();
                            });
    EXPECT_FALSE(result);
    EXPECT_EQ(calls, 0);
}

TEST(ContextCompaction, CompactsLegacyEmptyContentToolUseWithItsMatchingResult) {
    ToolUseMessage call{};
    call.id = MessageId{"legacy-call-id"};
    call.tool_name = "Read";
    call.tool_input_json =
        "{\"path\":\"/late/exact.cpp\",\"context\":\"" + std::string(8000, 'p') + "\"}";
    ASSERT_TRUE(call.content.empty());
    ToolResultMessage result{};
    result.id = MessageId{"legacy-result-id"};
    result.tool_use_id = ToolUseId{"legacy-call-id"};
    result.tool_name = "Read";
    result.content.emplace_back(TextBlock{std::string(14000, 'r') + " completed result=47"});
    std::vector<Message> messages{user("original task"), call, result,
                                  user("CURRENT_REQUEST_EXACT")};
    bool saw_arguments = false;
    const auto compacted = cc::compact_context(
        {.messages = messages, .request_anchor = 3}, policy(),
        [&](const cc::SummaryRequest& request) -> Result<cc::SummaryResponse> {
            saw_arguments |= request.transcript.find("/late/exact.cpp") != std::string::npos;
            return complete(checkpoint("Keep completed result=47; /late/exact.cpp"));
        });
    ASSERT_TRUE(compacted) << compacted.error().format();
    EXPECT_TRUE(saw_arguments);
    EXPECT_TRUE(std::ranges::contains(compacted->retained_indices, 3u));
    EXPECT_EQ(std::ranges::contains(compacted->retained_indices, 1u),
              std::ranges::contains(compacted->retained_indices, 2u));
    EXPECT_LT(compacted->after_tokens, compacted->before_tokens);
}
