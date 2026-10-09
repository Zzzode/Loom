/// @file streaming_replay.hpp
/// @brief Streaming payload replay harness for RFC 0003.
///
/// Loads JSONL event fixtures, injects core::StreamEvent values directly
/// into AppAdapter's event handler (bypassing the query thread / HTTP /
/// wire layers), simulates engine commits, manages query_running_ state,
/// and captures normalized screen snapshots at named checkpoints.
///
/// Layer 2 of the RFC 0003 architecture:
///   Layer 1 (fixtures)  → tests/fixtures/streaming_sessions/*.jsonl
///   Layer 2 (harness)   → this file
///   Layer 3 (assertion) → test_streaming_replay.cpp (golden + invariants)
///
/// Usage:
///   auto steps = loom::testing::load_fixture("text_simple");
///   loom::testing::StreamReplayHarness harness(app, engine);
///   auto snapshots = harness.play(steps);
///   // snapshots["after_delta"] → normalized screen text
///
/// NOTE: This header contains `import` declarations for loom modules
/// (loom.types.types, loom.serdes.json, …) because those types have no
/// textual headers — they exist only as C++23 modules. The including TU
/// does not need to re-import them. Textual standard-library and FTXUI
/// headers are included normally above the imports.

#pragma once

// ── Textual standard-library includes ──────────────────────────────────
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ── Textual FTXUI includes (headless screen capture) ──────────────────
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

// ── Shared screen normalization ───────────────────────────────────────
#include "screen_normalize.hpp"

// ── Loom module imports ───────────────────────────────────────────────
// These types are only available via C++23 named modules (no textual
// headers exist for them). The imports are TU-scoped: any .cpp that
// includes this header gets them automatically.
import loom.types.types;
import loom.serdes.json;
import loom.ui.app.app;
import loom.query.query_engine;
import loom.ui.messages.messages_list;
import loom.ui.foundation.clock;

namespace loom::testing {

namespace fs = std::filesystem;

// ============================================================
// Replay step types
// ============================================================

/// The kind of a single replay step.
enum class ReplayStepKind {
    Event,       ///< A core::StreamEvent to inject into the App handler.
    Commit,      ///< A committed message (assistant or tool result).
    EndQuery,    ///< Simulates the query thread finishing.
    Checkpoint,  ///< Captures a screen snapshot under checkpoint_name.
};

/// A single step in a replay fixture.
///
/// Only the field relevant to `kind` is populated:
///   Event      → event
///   Commit     → message
///   Checkpoint → checkpoint_name
///   EndQuery   → (none)
struct ReplayStep {
    ReplayStepKind kind = ReplayStepKind::Event;
    std::optional<loom::core::StreamEvent> event;
    std::optional<loom::core::Message> message;
    std::string checkpoint_name;
};

// ============================================================
// Fixture parse error
// ============================================================

/// Thrown when a fixture file cannot be opened or a line cannot be
/// parsed. The message includes the line number and JSON context.
class FixtureParseError : public std::runtime_error {
public:
    FixtureParseError(std::size_t line, std::string_view msg)
        : std::runtime_error(
              std::format("fixture line {}: {}", line, msg)) {}
};

// ============================================================
// Fixture deserialization (detail)
// ============================================================

namespace detail {

/// Parse a ContentBlock from a JSON value.
///
/// Supports the block types from RFC 0003 §6.1:
///   text, thinking, tool_use, tool_result, image, document.
[[nodiscard]] inline loom::core::ContentBlock parse_content_block(
    const loom::utils::json::JsonVal& block, std::size_t line_no) {
    const auto type = block.get_string("type");

    if (type == "text") {
        return loom::core::TextBlock{
            .text = block.get_string("text"),
        };
    }
    if (type == "thinking") {
        return loom::core::ThinkingBlock{
            .thinking = block.get_string("thinking"),
            .signature = block.get_string("signature"),
        };
    }
    if (type == "tool_use") {
        return loom::core::ToolUseBlock{
            .id = loom::core::ToolUseId{.value = block.get_string("id")},
            .name = block.get_string("name"),
            .input_json = block.get_string("input_json"),
        };
    }
    if (type == "tool_result") {
        loom::core::ToolResultBlock trb;
        trb.tool_use_id = loom::core::ToolUseId{
            .value = block.get_string("tool_use_id")};
        if (const auto ie = block.get("is_error");
            ie.valid() && ie.is_bool())
            trb.is_error = ie.as_bool();
        // Content is either a plain string or an array of content items
        // (text / image), mirroring the ToolResultBlock variant.
        const auto content = block.get("content");
        if (content.is_str()) {
            trb.content = std::string(content.as_str());
        } else if (content.is_arr()) {
            std::vector<loom::core::ToolResultContentItem> items;
            items.reserve(content.size());
            for (std::size_t i = 0; i < content.size(); ++i) {
                const auto item = content.at(i);
                if (!item.is_obj()) {
                    throw FixtureParseError(
                        line_no,
                        "tool_result: content item is not an object");
                }
                loom::core::ToolResultContentItem ci;
                ci.type = item.get_string("type");
                if (ci.type != "text" && ci.type != "image") {
                    throw FixtureParseError(
                        line_no,
                        std::format(
                            "tool_result: unknown content item type '{}'",
                            ci.type));
                }
                if (const auto t = item.get("text"); t.is_str())
                    ci.text = std::string(t.as_str());
                if (const auto mt = item.get("media_type"); mt.is_str())
                    ci.media_type = std::string(mt.as_str());
                if (const auto d = item.get("data"); d.is_str())
                    ci.data = std::string(d.as_str());
                items.push_back(std::move(ci));
            }
            trb.content = std::move(items);
        } else {
            throw FixtureParseError(
                line_no,
                "tool_result: 'content' must be a string or an array");
        }
        return trb;
    }
    if (type == "image") {
        loom::core::ImageBlock ib;
        ib.media_type = block.get_string("media_type");
        ib.data = block.get_string("data");
        if (const auto w = block.get("width"); w.valid() && w.is_num())
            ib.width = static_cast<std::size_t>(w.as_int());
        if (const auto h = block.get("height"); h.valid() && h.is_num())
            ib.height = static_cast<std::size_t>(h.as_int());
        return ib;
    }
    if (type == "document") {
        return loom::core::DocumentBlock{
            .media_type = block.get_string("media_type"),
            .data = block.get_string("data"),
        };
    }

    throw FixtureParseError(
        line_no, std::format("unknown content block type '{}'", type));
}

/// Parse a core::StreamEvent from a JSON object.
///
/// The `type` field has already been read and confirmed to be a
/// streaming event type (not a pseudo-event).
[[nodiscard]] inline loom::core::StreamEvent parse_stream_event(
    const loom::utils::json::JsonVal& root,
    std::string_view type,
    std::size_t line_no) {

    if (type == "stream_start") {
        return loom::core::StreamStart{
            .message_id = loom::core::MessageId{
                .value = root.get_string("message_id")},
            .model = root.get_string("model"),
        };
    }

    if (type == "content_block_start") {
        const auto block_val = root.get("block");
        if (!block_val.is_obj()) {
            throw FixtureParseError(
                line_no,
                "content_block_start: 'block' is missing or not an object");
        }
        return loom::core::ContentBlockStart{
            .index = static_cast<std::uint32_t>(root.get_int("index")),
            .block = parse_content_block(block_val, line_no),
        };
    }

    if (type == "content_block_delta") {
        return loom::core::ContentBlockDelta{
            .index = static_cast<std::uint32_t>(root.get_int("index")),
            .delta_text = root.get_string("delta_text"),
        };
    }

    if (type == "content_block_stop") {
        return loom::core::ContentBlockStop{
            .index = static_cast<std::uint32_t>(root.get_int("index")),
        };
    }

    if (type == "tool_execution_start") {
        return loom::core::ToolExecutionStart{
            .tool_use_id = root.get_string("tool_use_id"),
            .tool_name = root.get_string("tool_name"),
            .input_json = root.get_string("input_json"),
        };
    }

    if (type == "tool_execution_progress") {
        return loom::core::ToolExecutionProgress{
            .tool_use_id = root.get_string("tool_use_id"),
            .partial_result = root.get_string("partial_result"),
        };
    }

    if (type == "tool_execution_end") {
        const auto ie = root.get("is_error");
        return loom::core::ToolExecutionEnd{
            .tool_use_id = root.get_string("tool_use_id"),
            .result = root.get_string("result"),
            .is_error = ie.valid() && ie.is_bool() ? ie.as_bool() : false,
        };
    }

    if (type == "stream_end") {
        loom::core::StreamEnd se;
        // stop_reason may be null, missing, or a string.
        if (const auto sr = root.get("stop_reason"); sr.is_str())
            se.stop_reason = std::string(sr.as_str());
        if (const auto usage = root.get("usage"); usage.is_obj()) {
            se.usage.input_tokens =
                static_cast<std::uint32_t>(usage.get_int("input_tokens"));
            se.usage.output_tokens =
                static_cast<std::uint32_t>(usage.get_int("output_tokens"));
            if (const auto cc = usage.get("cache_creation_tokens");
                cc.valid() && cc.is_num())
                se.usage.cache_creation_tokens =
                    static_cast<std::uint32_t>(cc.as_int());
            if (const auto cr = usage.get("cache_read_tokens");
                cr.valid() && cr.is_num())
                se.usage.cache_read_tokens =
                    static_cast<std::uint32_t>(cr.as_int());
        }
        return se;
    }

    if (type == "stream_error") {
        return loom::core::StreamError{
            .error_type = root.get_string("error_type"),
            .message = root.get_string("message"),
        };
    }

    throw FixtureParseError(
        line_no, std::format("unknown stream event type '{}'", type));
}

/// Parse a single JSONL line into a ReplayStep.
[[nodiscard]] inline ReplayStep parse_step(
    const loom::utils::json::JsonVal& root, std::size_t line_no) {

    const auto type = root.get_string("type");

    // ── Pseudo-events ──────────────────────────────────────────────

    if (type == "__commit__") {
        const auto msg_val = root.get("message");
        if (!msg_val.is_obj()) {
            throw FixtureParseError(
                line_no, "__commit__: 'message' is missing or not an object");
        }

        // Role discriminator: "assistant" (default, for backward
        // compatibility with fixtures that omit it) or "tool".
        const auto role_val = msg_val.get("role");
        const std::string_view role = role_val.is_str()
                                          ? role_val.as_str()
                                          : std::string_view{"assistant"};

        if (role == "tool") {
            loom::core::ToolResultMessage msg;
            if (const auto id = msg_val.get("tool_use_id"); id.is_str())
                msg.tool_use_id = loom::core::ToolUseId{
                    .value = std::string(id.as_str())};
            if (const auto name = msg_val.get("tool_name"); name.is_str())
                msg.tool_name = std::string(name.as_str());
            if (const auto ie = msg_val.get("is_error");
                ie.valid() && ie.is_bool())
                msg.is_error = ie.as_bool();
            if (const auto content = msg_val.get("content");
                content.is_arr()) {
                for (std::size_t i = 0; i < content.size(); ++i) {
                    auto block = parse_content_block(content.at(i), line_no);
                    if (const auto* trb = std::get_if<
                            loom::core::ToolResultBlock>(&block)) {
                        // A ToolResultBlock is the API/history wire shape;
                        // the engine's ToolResultMessage stores
                        // TextBlock/ImageBlock directly (see
                        // query_engine_tools.cpp). Flatten it so the
                        // committed message renders identically to an
                        // engine-produced one.
                        if (msg.tool_use_id.empty() &&
                            !trb->tool_use_id.empty())
                            msg.tool_use_id = trb->tool_use_id;
                        if (trb->is_error) msg.is_error = true;
                        if (const auto* s =
                                std::get_if<std::string>(&trb->content)) {
                            msg.content.push_back(
                                loom::core::TextBlock{*s});
                        } else {
                            for (const auto& item : std::get<std::vector<
                                     loom::core::ToolResultContentItem>>(
                                     trb->content)) {
                                if (item.type == "text") {
                                    msg.content.push_back(
                                        loom::core::TextBlock{item.text});
                                } else if (item.type == "image") {
                                    loom::core::ImageBlock ib;
                                    ib.media_type = item.media_type;
                                    ib.data = item.data;
                                    msg.content.push_back(std::move(ib));
                                }
                            }
                        }
                    } else {
                        msg.content.push_back(std::move(block));
                    }
                }
            }
            return ReplayStep{
                .kind = ReplayStepKind::Commit,
                .event = std::nullopt,
                .message = loom::core::Message{std::move(msg)},
                .checkpoint_name = {},
            };
        }

        if (role == "user") {
            loom::core::UserMessage msg;
            if (const auto content = msg_val.get("content"); content.is_arr()) {
                for (std::size_t i = 0; i < content.size(); ++i) {
                    msg.content.push_back(
                        parse_content_block(content.at(i), line_no));
                }
            }
            return ReplayStep{
                .kind = ReplayStepKind::Commit,
                .event = std::nullopt,
                .message = loom::core::Message{std::move(msg)},
                .checkpoint_name = {},
            };
        }

        if (role != "assistant") {
            throw FixtureParseError(
                line_no,
                std::format("__commit__: unknown role '{}'", role));
        }

        loom::core::AssistantMessage msg;
        if (const auto m = msg_val.get("model"); m.is_str())
            msg.model = std::string(m.as_str());
        if (const auto sr = msg_val.get("stop_reason"); sr.is_str())
            msg.stop_reason = std::string(sr.as_str());
        if (const auto content = msg_val.get("content"); content.is_arr()) {
            for (std::size_t i = 0; i < content.size(); ++i) {
                msg.content.push_back(
                    parse_content_block(content.at(i), line_no));
            }
        }
        return ReplayStep{
            .kind = ReplayStepKind::Commit,
            .event = std::nullopt,
            .message = loom::core::Message{std::move(msg)},
            .checkpoint_name = {},
        };
    }

    if (type == "__end_query__") {
        return ReplayStep{
            .kind = ReplayStepKind::EndQuery,
            .event = std::nullopt,
            .message = std::nullopt,
            .checkpoint_name = {},
        };
    }

    if (type == "__checkpoint__") {
        return ReplayStep{
            .kind = ReplayStepKind::Checkpoint,
            .event = std::nullopt,
            .message = std::nullopt,
            .checkpoint_name = root.get_string("name"),
        };
    }

    // ── Streaming events ───────────────────────────────────────────
    return ReplayStep{
        .kind = ReplayStepKind::Event,
        .event = parse_stream_event(root, type, line_no),
        .message = std::nullopt,
        .checkpoint_name = {},
    };
}

}  // namespace detail

// ============================================================
// Fixture loading
// ============================================================

/// Load a fixture file and parse it into replay steps.
///
/// The fixture path is resolved relative to
/// `tests/fixtures/streaming_sessions/<fixture_name>.jsonl`
/// (relative to this header's location).
///
/// Each line is a JSON object with a `"type"` discriminator (RFC 0003
/// §6). Blank lines and lines starting with `#` are skipped.
///
/// @throws FixtureParseError on file-not-found or parse error, with the
///         line number and JSON context in the message.
[[nodiscard]] inline std::vector<ReplayStep> load_fixture(
    std::string_view fixture_name) {

    const auto path = fs::path(LOOM_TESTS_DIR) / "fixtures" /
                      "streaming_sessions" /
                      (std::string(fixture_name) + ".jsonl");

    std::ifstream file(path);
    if (!file) {
        throw FixtureParseError(
            0, std::format("cannot open fixture '{}' at {}",
                           fixture_name, path.string()));
    }

    std::vector<ReplayStep> steps;
    std::string line;
    std::size_t line_no = 0;

    while (std::getline(file, line)) {
        ++line_no;

        // Strip trailing \r (CRLF line endings).
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip blank lines and comments.
        const auto first_non_ws = line.find_first_not_of(" \t");
        if (first_non_ws == std::string::npos ||
            line[first_non_ws] == '#')
            continue;

        auto parsed = loom::utils::json::parse(line);
        if (!parsed) {
            throw FixtureParseError(
                line_no,
                std::format("JSON parse error: {}", parsed.error().message()));
        }

        const auto root = parsed->root();
        if (!root.is_obj()) {
            throw FixtureParseError(
                line_no, "line is not a JSON object");
        }

        steps.push_back(detail::parse_step(root, line_no));
    }

    return steps;
}

// ============================================================
// StreamReplayHarness
// ============================================================

/// The replay harness: injects events into an App, simulates commits,
/// manages query_running_, and captures screens.
///
/// Construct with the app and engine to drive. The engine is needed for
/// __commit__ (append_message_for_testing).
///
/// Typical usage:
///   StreamReplayHarness harness(app, engine);
///   auto snapshots = harness.play(steps);
///
/// Or with invariant checks:
///   auto snapshots = harness.play_with_invariants(steps,
///       [](std::string_view screen, std::size_t step) {
///           EXPECT_NO_FATAL_FAILURE(check_invariants(screen));
///       });
class StreamReplayHarness {
public:
    StreamReplayHarness(loom::ui::AppAdapter& app,
                        loom::core::QueryEngine& engine)
        : app_(app), engine_(engine) {}

    /// Render the app to a normalized screen string.
    ///
    /// Lets a test re-render after play() returns — e.g. a grace-expiry
    /// test that advances the clock via set_steady_now_for_testing() and
    /// then checks the collapsed thinking label (RFC 0003 §8.3, INV-07).
    [[nodiscard]] std::string render_now(int width = 120,
                                         int height = 40) const {
        return capture_screen(width, height);
    }

    /// Play all steps. At each checkpoint, captures the rendered screen
    /// (normalized) and stores it in the returned map.
    ///
    /// @param initial_steady_now  When set, installed as the process-global
    ///   steady-clock override AFTER the harness clears any prior override,
    ///   so every event/commit in this fixture is replayed at that fixed
    ///   time. Used by grace-expiry tests (RFC 0003 §8.3) that advance the
    ///   clock after play() returns.
    ///
    /// @return map of checkpoint_name → normalized screen text.
    [[nodiscard]] std::map<std::string, std::string> play(
        std::span<const ReplayStep> steps,
        int width = 120,
        int height = 40,
        std::optional<std::chrono::steady_clock::time_point>
            initial_steady_now = std::nullopt) {
        return play_with_invariants(steps, nullptr, width, height,
                                    initial_steady_now);
    }

    /// Play all steps, calling `invariant_check` after every step.
    ///
    /// The checker receives the normalized rendered screen and the step
    /// index. It is called after every step (Event, Commit, EndQuery,
    /// Checkpoint), not just Event steps.
    ///
    /// @return map of checkpoint_name → normalized screen text.
    [[nodiscard]] std::map<std::string, std::string> play_with_invariants(
        std::span<const ReplayStep> steps,
        std::function<void(std::string_view, std::size_t)> invariant_check,
        int width = 120,
        int height = 40,
        std::optional<std::chrono::steady_clock::time_point>
            initial_steady_now = std::nullopt) {

        // Clear the process-global thinking-stream grace map so that
        // was_recently_streaming() does not leak state across fixtures
        // (RFC 0003 §7.2).
        loom::ui::messages_list::clear_thinking_stream_last_seen_for_testing();

        // Clear the process-global steady-clock override so a previous
        // fixture's simulated time does not leak into this one (RFC 0003
        // §8.3). Fixtures that exercise grace-period expiry install their
        // own override via set_steady_now_for_testing().
        loom::ui::clock::set_steady_now_for_testing(std::nullopt);

        // Optionally pin the clock to a fixed time for the whole replay so
        // grace-expiry tests can advance it after play() returns.
        if (initial_steady_now) {
            loom::ui::clock::set_steady_now_for_testing(initial_steady_now);
        }

        std::map<std::string, std::string> snapshots;
        bool query_running = false;

        for (std::size_t i = 0; i < steps.size(); ++i) {
            const auto& step = steps[i];

            switch (step.kind) {
                case ReplayStepKind::Event:
                    // Set query_running_ before the first event, simulating
                    // HandleSubmit (RFC 0003 §5.2).
                    if (!query_running) {
                        loom::ui::test_seams(&app_)
                            .set_query_running_for_testing(true);
                        query_running = true;
                    }
                    loom::ui::test_seams(&app_)
                        .inject_stream_event_for_testing(*step.event);
                    break;

                case ReplayStepKind::Commit:
                    // Simulate the engine appending a committed message
                    // (Path B: streaming ↔ committed interaction).
                    engine_.append_message_for_testing(*step.message);
                    // A live query's on_commit callback invalidates projection.
                    loom::ui::test_seams(&app_)
                        .notify_conversation_changed_for_testing();
                    break;

                case ReplayStepKind::EndQuery:
                    // Simulate the query thread finishing. The next
                    // Render() takes the idle path (ConsumePendingResult →
                    // SyncState), clearing streaming state.
                    loom::ui::test_seams(&app_)
                        .set_query_running_for_testing(false);
                    query_running = false;
                    break;

                case ReplayStepKind::Checkpoint:
                    // Captured below.
                    break;
            }

            // Capture the screen for checkpoints and invariant checks.
            if (step.kind == ReplayStepKind::Checkpoint ||
                static_cast<bool>(invariant_check)) {
                const std::string screen = capture_screen(width, height);
                if (step.kind == ReplayStepKind::Checkpoint) {
                    snapshots[step.checkpoint_name] = screen;
                }
                if (invariant_check) {
                    invariant_check(screen, i);
                }
            }
        }

        return snapshots;
    }

private:
    loom::ui::AppAdapter& app_;
    loom::core::QueryEngine& engine_;

    /// Render the app to a fixed-size screen and return the normalized
    /// plain-text representation.
    [[nodiscard]] std::string capture_screen(int width, int height) const {
        auto screen = ftxui::Screen::Create(
            ftxui::Dimension::Fixed(width),
            ftxui::Dimension::Fixed(height));
        ftxui::Render(screen, app_.Render());
        return normalize_screen(screen.ToString());
    }
};

}  // namespace loom::testing
