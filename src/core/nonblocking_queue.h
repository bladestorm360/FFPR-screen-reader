#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

// Bounded MPMC ownership transfer. Four reservation attempts, never a wait/spin
// for another producer. Unavailable slots fail explicitly without game stalls.
template<class T, size_t Capacity> class NonblockingQueue {
    struct Slot { std::atomic<size_t> sequence; T value; };
    std::array<Slot, Capacity> slots_;
    std::atomic<size_t> head_{0}, tail_{0};
public:
    NonblockingQueue() { for (size_t i = 0; i < Capacity; ++i) slots_[i].sequence = i; }
    bool push(T&& value) {
        size_t pos = tail_.load(std::memory_order_relaxed);
        for (int attempt = 0; attempt < 4; ++attempt) {
            auto& slot = slots_[pos % Capacity];
            const auto difference = static_cast<ptrdiff_t>(slot.sequence.load(std::memory_order_acquire) - pos);
            if (difference < 0) return false;
            if (difference == 0 && tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                slot.value = std::move(value);
                slot.sequence.store(pos + 1, std::memory_order_release);
                return true;
            }
            if (difference > 0) pos = tail_.load(std::memory_order_relaxed);
        }
        return false;
    }
    bool pop(T& value) {
        size_t pos = head_.load(std::memory_order_relaxed);
        for (int attempt = 0; attempt < 4; ++attempt) {
            auto& slot = slots_[pos % Capacity];
            const auto difference = static_cast<ptrdiff_t>(slot.sequence.load(std::memory_order_acquire) - (pos + 1));
            if (difference < 0) return false;
            if (difference == 0 && head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                value = std::move(slot.value);
                slot.sequence.store(pos + Capacity, std::memory_order_release);
                return true;
            }
            if (difference > 0) pos = head_.load(std::memory_order_relaxed);
        }
        return false;
    }
    size_t size() const { return tail_.load() - head_.load(); }
};
