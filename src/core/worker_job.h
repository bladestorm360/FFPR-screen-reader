#pragma once
#include "background.h"
#include "frame.h"
#include <atomic>
#include <climits>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <optional>

// Owned snapshots only on workers. Native-owner callbacks remain in the main
// registry; even a callback holding a Unity pointer is never transferred to work.
namespace worker_job {
class Owner {
public:
    Owner() = default;
    Owner(const Owner&) = delete;
    ~Owner() { cancel(); }
    // Transition admission must not destroy captured native-owner callbacks.
    // Retire them with cancel() at the next safe lifecycle boundary.
    void interrupt() { ++*epoch_; }
    void cancel() {
        interrupt();
        for (auto& [id, erase] : erase_) erase();
        erase_.clear();
    }
    std::shared_ptr<std::atomic<uint64_t>> epoch() const { return epoch_; }
    uint64_t retain(std::function<void()> erase) {
        const auto id = ++next_;
        erase_.emplace(id, std::move(erase));
        return id;
    }
    void release(uint64_t id) { erase_.erase(id); }
private:
    std::shared_ptr<std::atomic<uint64_t>> epoch_ = std::make_shared<std::atomic<uint64_t>>(0);
    uint64_t next_ = 0;
    std::map<uint64_t, std::function<void()>> erase_;
};
template<class Result> struct Registry {
    inline static uint64_t next = 0;
    inline static std::map<uint64_t, std::function<void(Result)>> callbacks;
};
template<class Snapshot, class Work, class Publish>
bool start(Owner& owner, Snapshot snapshot, Work work, Publish publish,
           background::Lane lane = background::Lane::Compute)
{
    using Result = decltype(work(snapshot));
    using R = Registry<Result>;
    const uint64_t id = ++R::next;
    auto epoch = owner.epoch();
    const uint64_t generation = epoch->load();
    const auto retained = owner.retain([id] { R::callbacks.erase(id); });
    R::callbacks.emplace(id, [&owner, retained, publish = std::move(publish)](Result result) mutable {
        owner.release(retained);
        publish(std::move(result));
    });
    auto job = std::make_shared<std::function<void()>>([id, epoch, generation, snapshot = std::move(snapshot), work = std::move(work)]() mutable {
        if (epoch->load() != generation) return;
        std::optional<Result> result;
        try { result.emplace(work(snapshot)); }
        catch (...) { result.emplace(); } // owner continuation still retires its callback
        // Cancellation during computation also retires the owned snapshot/result
        // here. Do not enqueue a large obsolete result for game-thread destruction.
        if (epoch->load() != generation) return;
        frame::post([id, epoch, generation, result = std::move(*result)]() mutable {
            auto callback = R::callbacks.find(id);
            if (callback == R::callbacks.end()) return;
            auto publish = std::move(callback->second);
            R::callbacks.erase(callback);
            if (epoch->load() == generation) publish(std::move(result));
        });
    });
    auto attempt = [job, lane, epoch, generation] {
        if (epoch->load() != generation) return false;
        // Keep the owned job when a lane is busy/full. Never lose it or run inline.
        return !background::post([job] { (*job)(); }, lane);
    };
    if (attempt()) frame::every_frame(std::move(attempt), "WorkerJob.Publish", INT_MAX);
    return true;
}
}
