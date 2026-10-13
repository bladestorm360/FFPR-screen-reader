#pragma once
#include "../say.h"

namespace ui {
// Owner-thread native-entry observation: nested ready focus callbacks can precede
// the parent's ready headline. Retain only owned text and publish in native order
// after initialization returns. No backend call, timer, wait or text deduplication.
class InitialSpeech {
    bool collecting_ = false;
    std::vector<std::wstring> focus_;
public:
    void begin() { collecting_ = true; focus_.clear(); }
    bool capture(const std::wstring& text) {
        if (!collecting_) return false;
        focus_.push_back(text); return true;
    }
    void ready(std::wstring headline) {
        collecting_ = false;
        std::vector<std::wstring> parts;
        parts.push_back(std::move(headline));
        for (auto& text : focus_) parts.push_back(std::move(text));
        focus_.clear();
        ffpr::say_sequence(std::move(parts), true);
    }
};
}
