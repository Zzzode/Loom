/// @file command_registry_init_c.cpp
/// @brief Group C registration: session/model/plan commands (model, cost, plan, insights, etc.)
module loom.commands.registry;

import loom.commands.upgrade;
import loom.commands.ultraplan;
import loom.commands.review.ultrareview;
import loom.commands.review.review_remote;
import loom.commands.security_review;
import loom.commands.init_verifiers;
import loom.commands.install;
import loom.commands.insights;
import loom.commands.init;
import loom.commands.session;
import loom.commands.resume;
import loom.commands.model;
import loom.commands.cost;
import loom.commands.plan;
import loom.commands.theme;
import loom.commands.vim;

namespace loom::commands {

void register_group_c_commands(CommandRegistry& registry) {
    registry.register_command<UpgradeCommand>();
    registry.register_command<UltraplanCommand>();
    registry.register_command<UltraReviewCommand>();
    registry.register_command<ReviewRemoteCommand>();
    registry.register_command<SecurityReviewCommand>();
    registry.register_command<InitVerifiersCommand>();
    registry.register_command<InstallCommand>();
    registry.register_command<InsightsCommand>();
    registry.register_command<InitCommand>();
    registry.register_command<SessionCommand>();
    registry.register_command<ResumeCommand>();
    registry.register_command<ModelCommand>();
    registry.register_command<CostCommand>();
    registry.register_command<PlanCommand>();
    registry.register_command<ThemeCommand>();
    registry.register_command<VimCommand>();
}

} // namespace loom::commands