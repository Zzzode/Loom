// paste_coordinator.cpp — impl unit for loom.ui.app.paste_coordinator

module;

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

module loom.ui.app.paste_coordinator;

import std;

import loom.types.types;
import loom.platform.clipboard;
import loom.crypto.crypto;

namespace loom::ui {

void PasteCoordinator::SpawnPasteWorker(int id,
                                        std::function<void()> post_render,
                                        bool no_real_paste_worker) {
    // Mark this id as in-flight (main thread — only main thread touches
    // in_flight_pastes_). Cleared by ProcessCompletedPastes when the
    // result/failure lands.
    mark_in_flight(id);

    // Testing short-circuit: inject a fake image synchronously, no thread.
    if (no_real_paste_worker) {
        loom::core::ImageBlock ib;
        ib.media_type = "image/png";
        ib.data = "iVBORw0KGgo=";
        ib.size_bytes = 100;
        ib.file_name = "test_" + std::to_string(id) + ".png";
        ib.source = loom::core::ImageBlockSource::Clipboard;
        {
            std::lock_guard lk(paste_mutex_);
            pending_paste_results_[id] = std::move(ib);
        }
        post_render();
        return;
    }

    // Capture only what the thread needs by value.  `this` is safe
    // because AppAdapter outlives any paste worker (the app object
    // lives for the whole session).
    std::thread([this, id, post_render = std::move(post_render)]() {
        // Skip has_image() — it costs an extra 600ms osascript call.
        // Just try read_image_png() directly; it returns nullopt if
        // there's no image in the clipboard.  This cuts total paste
        // latency from ~1.3s (2 osascript calls) to ~650ms (1 call).
        auto png = loom::utils::clipboard::read_image_png();
        if (!png || png->empty()) {
            // No image in clipboard — try reading plain text instead.
            std::string clip_text = loom::utils::clipboard::read_text();
            if (!clip_text.empty()) {
                std::lock_guard lk(paste_mutex_);
                pending_paste_text_results_[id] = std::move(clip_text);
            } else {
                std::lock_guard lk(paste_mutex_);
                pending_paste_failures_.insert(id);
            }
            post_render();
            return;
        }
        // Build the loom::core::ImageBlock on the worker thread (base64 encode can
        // be non-trivial for large screenshots).
        const std::size_t raw_bytes = png->size();
        auto t = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);
        char fname[48];
        std::strftime(fname, sizeof(fname),
                      "clipboard %Y%m%d-%H%M%S.png", &tm_buf);
        loom::core::ImageBlock ib;
        ib.media_type = "image/png";
        ib.data = loom::utils::crypto::base64_encode(
            png->data(), png->size());
        ib.size_bytes = raw_bytes;
        ib.file_name  = std::string(fname);
        ib.source     = loom::core::ImageBlockSource::Clipboard;

        {
            std::lock_guard lk(paste_mutex_);
            pending_paste_results_[id] = std::move(ib);
        }
        post_render();
    }).detach();
}

}  // namespace loom::ui
