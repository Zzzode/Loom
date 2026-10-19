// MessagesClient lifetime barrier for loom.services.api.client.
module;

module loom.services.api.client;

import std;

namespace loom::services::api {

Result<StreamParser> MessagesClient::create_message_stream(
    const CreateMessageRequest& request) {
    CreateMessageRequest stream_request = request;
    stream_request.stream = true;
    auto betas = betas_for_request(request);
    auto prepared = prepare_request(stream_request, betas, /*streaming=*/true);
    if (!prepared) return std::unexpected(prepared.error());
    return start_stream(std::move(prepared->url), std::move(prepared->body),
                        std::move(prepared->headers), {},
                        auth_ctx_->provider() != EProv::FirstParty);
}

Result<std::size_t> MessagesClient::prepared_stream_request_size(const CreateMessageRequest& request) {
    auto stream_request = request;
    stream_request.stream = true;
    auto prepared = prepare_request(stream_request, betas_for_request(request), true);
    if (!prepared) return std::unexpected(prepared.error());
    return prepared->body.size();
}

Result<StreamParser> MessagesClient::create_prepared_stream(
    std::string url, std::string body, std::vector<std::string> headers,
    StreamParser::EventDecoder decoder) {
    return start_stream(std::move(url), std::move(body), std::move(headers),
                        std::move(decoder), false);
}

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
