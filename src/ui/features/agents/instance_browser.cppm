/// Session-local agent instance navigation; App supplies scoped snapshots.
module;

#include <ftxui/component/component_base.hpp>

export module loom.ui.features.agents.instance_browser;

import std;
import loom.ui.features.agents.runtime_agents;

export namespace loom::ui::agents::instances {

struct Entry {
    std::string id;
    std::string name;
    std::string type;
    std::string description;
    std::string team_name;
    runtime::Status status = runtime::Status::Queued;
    bool team = false;
    bool active = false;
    bool cancelling = false;
};

struct Detail {
    Entry entry;
    std::string error;
    std::string output;
    std::string transcript;
    std::string continue_reason;
    std::string stop_reason;
    bool can_continue = false;
    bool can_stop = false;
};

struct BrowserState {
    int columns = 80;
    int rows = 24;
    bool history = false;
    bool team = false;
    std::string selected_id;
};

struct Options {
    bool open_detail = false;
    std::shared_ptr<BrowserState> state;
    std::function<std::vector<Entry>()> list;
    std::function<std::optional<Detail>(std::string_view)> detail;
    std::function<void(std::string, std::string)> on_continue;
    std::function<void(std::string)> on_stop;
    std::function<void(std::string)> on_open;
    std::function<void()> on_close;
};

[[nodiscard]] ftxui::Component CreateBrowser(Options options);

} // namespace loom::ui::agents::instances
