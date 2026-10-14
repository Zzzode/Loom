/// @file test_program_status.cpp
/// @brief OSC 7501 lifecycle and the production FTXUI input parser.
#include <gtest/gtest.h>
#include <ftxui/component/terminal_input_parser.hpp>

import std;
import loom.terminal.osc;
import loom.ui.app.program_status;

namespace {

namespace osc = loom::terminal::osc;
using Reporter = loom::ui::ProgramStatusReporter;
using State = osc::ProgramStatusState;
using Kind = osc::ProgramStatusKind;
using namespace std::chrono_literals;

struct Harness {
    std::vector<std::string> writes;
    bool succeed = true;
    bool throws = false;
    Reporter reporter{[this](std::string_view bytes) {
        writes.emplace_back(bytes);
        if (throws) throw std::runtime_error("injected terminal failure");
        return succeed;
    }};
    Reporter::Time now{};

    void enable(std::string_view reply = "\033]7501;?\033\\") {
        reporter.Start(true, now);
        ASSERT_TRUE(reporter.HandleControl(reply, now));
        reporter.Flush(now);
    }
    std::string report(State state, Kind kind = Kind::None) {
        return *osc::program_status(state, kind);
    }
};

struct ParserHarness {
    ftxui::Receiver<ftxui::Task> receiver = ftxui::MakeReceiver<ftxui::Task>();
    ftxui::TerminalInputParser parser{receiver->MakeSender()};

    void add(std::string_view bytes) {
        for (const auto byte : bytes) parser.Add(byte);
    }
    std::vector<ftxui::Event> drain() {
        std::vector<ftxui::Event> events;
        ftxui::Task task;
        while (receiver->ReceiveNonBlocking(&task)) {
            if (const auto* event = std::get_if<ftxui::Event>(&task)) events.push_back(*event);
        }
        return events;
    }
};

TEST(ProgramStatus, NoninteractiveNeverProbesOrReports) {
    Harness h;
    h.reporter.Start(false, h.now);
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now));
    h.reporter.Begin();
    h.reporter.Flush(h.now);
    h.reporter.Finish(true, h.now);
    EXPECT_TRUE(h.writes.empty());
}

TEST(ProgramStatus, ProbeOnceAndWaitSilentlyForReply) {
    Harness h;
    h.reporter.Start(true, h.now);
    h.reporter.Start(true, h.now);
    h.reporter.Begin();
    h.reporter.Flush(h.now + 999ms);
    ASSERT_EQ(h.writes.size(), 1u);
    EXPECT_EQ(h.writes[0], osc::program_status_query());
}

TEST(ProgramStatus, PreProbeReplyConsumedWithoutLatching) {
    Harness h;
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now));
    h.reporter.Start(true, h.now);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.size(), 1u);
}

TEST(ProgramStatus, BelAndStRepliesEnableOnlyAtFlush) {
    for (const auto reply : {"\033]7501;?\a", "\033]7501;?\033\\"}) {
        Harness h;
        h.reporter.Start(true, h.now);
        EXPECT_TRUE(h.reporter.HandleControl(reply, h.now + 1ms));
        ASSERT_EQ(h.writes.size(), 1u);
        h.reporter.Flush(h.now + 2ms);
        ASSERT_EQ(h.writes.size(), 2u);
        EXPECT_EQ(h.writes.back(), h.report(State::Idle));
    }
}

TEST(ProgramStatus, LatestDesiredSnapshotWinsWhileProbing) {
    Harness h;
    h.reporter.Start(true, h.now);
    h.reporter.Begin();
    h.reporter.Refresh(true, Kind::Permission);
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now));
    h.reporter.Flush(h.now);
    ASSERT_EQ(h.writes.size(), 2u);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Permission));
}

TEST(ProgramStatus, DeduplicatesIdenticalSnapshots) {
    Harness h;
    h.enable();
    h.reporter.Flush(h.now);
    h.reporter.Refresh(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.size(), 2u);
}

TEST(ProgramStatus, DeadlineLatchesAndLateRepliesAreConsumed) {
    Harness h;
    h.reporter.Start(true, h.now);
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now + 1000ms));
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now + 1001ms));
    h.reporter.Flush(h.now + 1001ms);
    EXPECT_EQ(h.writes.size(), 1u);
}

TEST(ProgramStatus, FlushExpiresProbeWithoutTimerOrNewInput) {
    Harness h;
    h.reporter.Start(true, h.now);
    h.reporter.Flush(h.now + 1000ms);
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now + 1000ms));
    h.reporter.Flush(h.now + 1000ms);
    EXPECT_EQ(h.writes.size(), 1u);
}

TEST(ProgramStatus, RestrictedStatesDisableRatherThanInventFallback) {
    Harness h;
    h.enable("\033]7501;?:states=idle,working,done,blocked\a");
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?\a", h.now));
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.size(), 1u);
}

TEST(ProgramStatus, RestrictedKindsPreserveBlockedWithoutUnsupportedKind) {
    Harness h;
    h.enable("\033]7501;?:kinds=question\a");
    h.reporter.Refresh(true, Kind::Permission);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked));
    h.reporter.Refresh(true, Kind::Question);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Question));
}

TEST(ProgramStatus, WhitespaceUnknownAndMalformedPairsAndLastDuplicate) {
    Harness h;
    h.enable("\033]7501; ? : unknown=value : broken : states=idle :"
             " states = idle,working,done,blocked,error,future : kinds= : kinds=permission \a");
    h.reporter.Refresh(false, Kind::Permission);
    h.reporter.Flush(h.now);
    ASSERT_EQ(h.writes.size(), 3u);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Permission));
}

TEST(ProgramStatus, InvalidPairsAreSkippedAndInvalidReplyDoesNotEnable) {
    Harness h;
    h.reporter.Start(true, h.now);
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;state=idle\a", h.now));
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?", h.now));
    h.reporter.Flush(h.now);
    ASSERT_EQ(h.writes.size(), 1u);
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?:States=idle:states=bad value\a", h.now));
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Idle));
}

TEST(ProgramStatus, ExactCommandBoundaryAndMalformedControlsAreConsumed) {
    Harness h;
    EXPECT_FALSE(h.reporter.HandleControl("ordinary", h.now));
    EXPECT_FALSE(h.reporter.HandleControl("\033]75010;?\a", h.now));
    EXPECT_FALSE(h.reporter.HandleControl("\033]52;c;?\a", h.now));
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501", h.now));
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501\a", h.now));
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;malformed\a", h.now));
}

TEST(ProgramStatus, OversizeReplyConsumedWithoutEnabling) {
    Harness h;
    h.reporter.Start(true, h.now);
    const std::string oversized = "\033]7501;?:x=" + std::string(4096, 'a') + '\a';
    EXPECT_TRUE(h.reporter.HandleControl(oversized, h.now));
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.size(), 1u);
    const std::string largest = "\033]7501;?:x=" + std::string(4084, 'a') + '\a';
    ASSERT_EQ(largest.size(), 4096u);
    EXPECT_TRUE(h.reporter.HandleControl(largest, h.now));
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Idle));
}

TEST(ProgramStatus, FirstAcceptedCapabilitiesCannotLaterBeRestricted) {
    Harness h;
    h.enable();
    EXPECT_TRUE(h.reporter.HandleControl("\033]7501;?:states=idle:kinds=\a", h.now));
    h.reporter.Refresh(false, Kind::Permission);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Permission));
}

TEST(ProgramStatus, OutcomeRetainedUntilAcknowledgedOrNextOperation) {
    Harness h;
    h.enable();
    h.reporter.Begin();
    h.reporter.Complete(false);
    h.reporter.Refresh(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Done));
    h.reporter.Refresh(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.size(), 3u);
    h.reporter.Acknowledge();
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Idle));
    h.reporter.Begin();
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Working));
}

TEST(ProgramStatus, BeginLatchesWorkUntilCompletionActionIsApplied) {
    Harness h;
    h.enable();
    h.reporter.Begin();
    h.reporter.Refresh(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Working));
    h.reporter.Complete(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Done));
}

TEST(ProgramStatus, ErrorRetainedAcrossBackgroundBlockRaiseAndResolve) {
    Harness h;
    h.enable();
    h.reporter.Begin();
    h.reporter.Complete(true);
    h.reporter.Refresh(false, Kind::Permission);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Permission));
    h.reporter.Refresh(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Error));
}

TEST(ProgramStatus, CancelWinsLateErrorButDoesNotEraseActualBlock) {
    Harness h;
    h.enable();
    h.reporter.Begin();
    h.reporter.Refresh(true, Kind::Permission);
    h.reporter.Cancel();
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Permission));
    h.reporter.Refresh(true);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Working));
    h.reporter.Complete(true);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Idle));
}

TEST(ProgramStatus, PausedContinuationIsQuestionUntilAcknowledged) {
    Harness h;
    h.enable();
    h.reporter.Begin();
    h.reporter.Complete(false, true);
    h.reporter.Refresh(false);
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Question));
    h.reporter.Acknowledge();
    h.reporter.Flush(h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Idle));
}

TEST(ProgramStatus, MutationsNeverWriteAndFinishPreservesDoneOrError) {
    for (const bool error : {false, true}) {
        Harness h;
        h.enable();
        h.reporter.Begin();
        h.reporter.Complete(error);
        h.reporter.Refresh(false);
        ASSERT_EQ(h.writes.size(), 2u);
        h.reporter.Finish(true, h.now);
        EXPECT_EQ(h.writes.back(), h.report(error ? State::Error : State::Done));
        const auto count = h.writes.size();
        h.reporter.Finish(false, h.now);
        h.reporter.Start(true, h.now);
        h.reporter.Begin();
        h.reporter.Flush(h.now);
        EXPECT_EQ(h.writes.size(), count);
    }
}

TEST(ProgramStatus, NormalFinishRetainsForegroundOutcomeUnderBackgroundBlock) {
    for (const bool error : {false, true}) {
        Harness h;
        h.enable();
        h.reporter.Begin();
        h.reporter.Complete(error);
        h.reporter.Refresh(false, Kind::Permission);
        h.reporter.Flush(h.now);
        ASSERT_EQ(h.writes.back(), h.report(State::Blocked, Kind::Permission));
        h.reporter.Finish(true, h.now);
        EXPECT_EQ(h.writes.back(), h.report(error ? State::Error : State::Done));
    }
}

TEST(ProgramStatus, AbnormalFinishClearsOutcomeAndActiveExitIsNeverDone) {
    Harness h;
    h.enable();
    h.reporter.Complete(false);
    h.reporter.Flush(h.now);
    h.reporter.Finish(false, h.now);
    EXPECT_EQ(h.writes.back(), h.report(State::Idle));
    Harness active;
    active.enable();
    active.reporter.Begin();
    active.reporter.Flush(active.now);
    active.reporter.Finish(true, active.now);
    EXPECT_EQ(active.writes.back(), active.report(State::Idle));
    Harness paused;
    paused.enable();
    paused.reporter.Complete(false, true);
    paused.reporter.Flush(paused.now);
    paused.reporter.Finish(true, paused.now);
    EXPECT_EQ(paused.writes.back(), paused.report(State::Idle));
}

TEST(ProgramStatus, ProbeAndReportFailuresDisablePermanently) {
    Harness probe;
    probe.succeed = false;
    probe.reporter.Start(true, probe.now);
    probe.succeed = true;
    EXPECT_TRUE(probe.reporter.HandleControl("\033]7501;?\a", probe.now));
    probe.reporter.Flush(probe.now);
    EXPECT_EQ(probe.writes.size(), 1u);
    Harness report;
    report.enable();
    report.succeed = false;
    report.reporter.Begin();
    report.reporter.Flush(report.now);
    report.succeed = true;
    report.reporter.Complete(false);
    report.reporter.Flush(report.now);
    report.reporter.Finish(true, report.now);
    EXPECT_EQ(report.writes.size(), 3u);
}

TEST(ProgramStatus, WriterExceptionsNeverEscapeFlushOrFinish) {
    Harness h;
    h.enable();
    h.throws = true;
    h.reporter.Begin();
    EXPECT_NO_THROW(h.reporter.Flush(h.now));
    EXPECT_NO_THROW(h.reporter.Finish(true, h.now));
    EXPECT_EQ(h.writes.size(), 3u);
}

TEST(ProgramStatusParser, BelAndStAreSingleOpaqueSpecialsFollowedByOrdinaryKeys) {
    for (const auto frame : {"\033]7501;?\a", "\033]7501;?\033\\"}) {
        ParserHarness h;
        h.add(frame);
        h.add("x");
        const auto events = h.drain();
        ASSERT_EQ(events.size(), 2u);
        EXPECT_EQ(events[0].input(), frame);
        EXPECT_FALSE(events[0].is_character());
        EXPECT_EQ(events[1], ftxui::Event::Character('x'));
    }
}

TEST(ProgramStatusParser, BelAfterMalformedEscapeEndsFrameBeforeFreshKey) {
    ParserHarness h;
    const std::string frame = "\033]7501;?:unknown=\033\a";
    h.add(frame + "x");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].input(), frame);
    EXPECT_FALSE(events[0].is_character());
    EXPECT_EQ(events[1], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, StAfterRepeatedEscapeEndsFrameBeforeFreshKey) {
    ParserHarness h;
    const std::string frame = "\033]7501;?:unknown=\033\033\\";
    h.add(frame + "x");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].input(), frame);
    EXPECT_FALSE(events[0].is_character());
    EXPECT_EQ(events[1], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, FragmentedHeaderAndPayloadRetainedBeyondFiftyMs) {
    ParserHarness h;
    h.add("\033]");
    h.parser.Timeout(100);
    EXPECT_TRUE(h.drain().empty());
    h.add("75");
    h.parser.Timeout(100);
    EXPECT_TRUE(h.drain().empty());
    h.add("01;?:states=idle,working,");
    h.parser.Timeout(100);
    EXPECT_TRUE(h.drain().empty());
    h.add("done,blocked,error\a");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].input(), "\033]7501;?:states=idle,working,done,blocked,error\a");
}

TEST(ProgramStatusParser, TotalTimeoutDoesNotResetAtEachFragment) {
    ParserHarness h;
    h.add("\033]7501;?");
    h.parser.Timeout(600);
    h.add(":states=idle");
    h.parser.Timeout(400);
    const auto expired = h.drain();
    ASSERT_EQ(expired.size(), 1u);
    EXPECT_FALSE(expired[0].is_character());
    h.add("x");
    const auto fresh = h.drain();
    ASSERT_EQ(fresh.size(), 1u);
    EXPECT_EQ(fresh[0], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, RealAgeExpiresWithoutTimeoutCalls) {
    ParserHarness h;
    h.add("\033]7501;?");
    std::this_thread::sleep_for(1010ms);
    h.add("x");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_FALSE(events[0].is_character());
    EXPECT_EQ(events[1], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, ExactByteLimitAcceptsCompleteFrame) {
    ParserHarness h;
    const std::string frame = "\033]7501;?:x=" + std::string(4084, 'a') + '\a';
    ASSERT_EQ(frame.size(), 4096u);
    h.add(frame);
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].input(), frame);
}

TEST(ProgramStatusParser, OversizeFrameDiscardsThroughBelWithoutCharacterReplay) {
    ParserHarness h;
    h.add("\033]7501;?:x=" + std::string(4100, 'a'));
    const auto truncated = h.drain();
    ASSERT_EQ(truncated.size(), 1u);
    EXPECT_EQ(truncated[0].input().size(), 4096u);
    EXPECT_FALSE(truncated[0].is_character());
    h.add("discarded\ax");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, OversizeTerminatorAcrossByteBoundaryRestoresInput) {
    ParserHarness h;
    const std::string frame = "\033]7501;?:x=" + std::string(4084, 'a') + "\033\\";
    ASSERT_EQ(frame.size(), 4097u);
    h.add(frame);
    h.add("x");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].input().size(), 4096u);
    EXPECT_FALSE(events[0].is_character());
    EXPECT_EQ(events[1], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, DiscardPreservesOriginalBudgetAcrossContinuousFragments) {
    ParserHarness h;
    h.add("\033]7501;?");
    h.parser.Timeout(600);
    h.add(std::string(4100, 'a'));
    ASSERT_EQ(h.drain().size(), 1u);
    h.parser.Timeout(400);
    EXPECT_TRUE(h.drain().empty());
    h.add("x");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], ftxui::Event::Character('x'));
}

TEST(ProgramStatusParser, OtherOscRetainsFiftyMsAndStOnlyBehavior) {
    ParserHarness h;
    h.add("\033]52;c;value\a");
    EXPECT_TRUE(h.drain().empty());
    h.parser.Timeout(50);
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].input(), "\033]52;c;value\a");
    h.add("\033]123;other\033\\");
    ASSERT_EQ(h.drain().size(), 1u);
    h.add("\033]7");
    h.parser.Timeout(100);
    EXPECT_TRUE(h.drain().empty());
    h.add(";other\a");
    h.parser.Timeout(49);
    EXPECT_TRUE(h.drain().empty());
    h.parser.Timeout(1);
    ASSERT_EQ(h.drain().size(), 1u);
}

TEST(ProgramStatusParser, StandaloneAndAdjacentEscapeRemainDistinct) {
    ParserHarness h;
    h.add("\033");
    h.parser.Timeout(49);
    EXPECT_TRUE(h.drain().empty());
    h.parser.Timeout(1);
    ASSERT_EQ(h.drain().size(), 1u);
    h.add("\033\033]7501;?\a");
    const auto events = h.drain();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0], ftxui::Event::Escape);
    EXPECT_EQ(events[1].input(), "\033]7501;?\a");
}

}  // namespace
