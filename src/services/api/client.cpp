// MessagesClient lifetime barrier for loom.services.api.client.
module;

module loom.services.api.client;

import std;

namespace loom::services::api {

MessagesClient::~MessagesClient() {
    // Streams may outlive this client, but must stop borrowing its auth
    // context before it is destroyed. Request all stops before any join.
    std::vector<std::shared_ptr<StreamTransfer>> transfers;
    {
        std::scoped_lock lock(transfers_mutex_);
        for (const auto& weak : transfers_) {
            if (auto transfer = weak.lock()) transfers.push_back(std::move(transfer));
        }
    }
    for (const auto& transfer : transfers) transfer->request_stop();
    for (const auto& transfer : transfers) {
        transfer->stop_and_join();
    }
}

} // namespace loom::services::api
