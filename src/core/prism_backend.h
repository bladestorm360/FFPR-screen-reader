#pragma once
#include <prism.h>
#include <string>
#include <string_view>
#include <cstring>

// Worker-owned adapter. Feature readers keep the backend-independent speech API.
namespace speech::prism {
#define FFPR_PRISM_API(X) \
    X(config_init) X(init) X(shutdown) X(registry_acquire_best) \
    X(backend_initialize) X(backend_name) X(backend_get_features) \
    X(backend_output) X(backend_speak) X(backend_braille) X(backend_stop) \
    X(backend_free) X(error_string) X(version_string)
struct Api {
#define FFPR_PRISM_MEMBER(n) decltype(&prism_##n) n = nullptr;
    FFPR_PRISM_API(FFPR_PRISM_MEMBER)
#undef FFPR_PRISM_MEMBER
    bool complete() const {
        bool okay = true;
#define FFPR_PRISM_PRESENT(n) okay = okay && n;
        FFPR_PRISM_API(FFPR_PRISM_PRESENT)
#undef FFPR_PRISM_PRESENT
        return okay;
    }
};
inline std::string utf8(std::wstring_view value) {
    if (value.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(length, '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), result.data(), length, nullptr, nullptr)) return {};
    return result;
}
class Backend {
    Api api_;
    PrismContext* context_ = nullptr;
    PrismBackend* backend_ = nullptr;
public:
    bool load(Api api) {
        close();
        if (!api.complete()) return false;
        const char* version = api.version_string();
        if (!version || std::strcmp(version, PRISM_VERSION_STRING) != 0) return false;
        api_ = api;
        PrismConfig config = api_.config_init();
        context_ = api_.init(&config);
        if (!context_) { close(); return false; }
        backend_ = api_.registry_acquire_best(context_);
        if (!backend_) { close(); return false; }
        const auto error = api_.backend_initialize(backend_);
        if ((error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) ||
            !(features() & PRISM_BACKEND_SUPPORTS_SPEAK)) { close(); return false; }
        return true;
    }
    void close() {
        if (backend_) api_.backend_free(backend_);
        backend_ = nullptr;
        if (context_) api_.shutdown(context_);
        context_ = nullptr;
        api_ = {};
    }
    uint64_t features() const { return backend_ ? api_.backend_get_features(backend_) : 0; }
    bool has_braille() const { return (features() & PRISM_BACKEND_SUPPORTS_BRAILLE) != 0; }
    const char* name() const { return backend_ ? api_.backend_name(backend_) : "(unavailable)"; }
    PrismError output(std::wstring_view value, bool interrupt, bool braille_only = false) {
        if (!backend_) return PRISM_ERROR_NOT_INITIALIZED;
        const auto encoded = utf8(value);
        if (encoded.empty()) return value.empty() ? PRISM_OK : PRISM_ERROR_INVALID_UTF8;
        const auto flags = features();
        if (braille_only) return flags & PRISM_BACKEND_SUPPORTS_BRAILLE ?
            api_.backend_braille(backend_, encoded.c_str()) : PRISM_ERROR_NOT_IMPLEMENTED;
        if (flags & PRISM_BACKEND_SUPPORTS_OUTPUT) return api_.backend_output(backend_, encoded.c_str(), interrupt);
        const auto spoken = api_.backend_speak(backend_, encoded.c_str(), interrupt);
        if (flags & PRISM_BACKEND_SUPPORTS_BRAILLE) {
            const auto written = api_.backend_braille(backend_, encoded.c_str());
            if (spoken == PRISM_OK) return written;
        }
        return spoken;
    }
    PrismError stop() { return backend_ && (features() & PRISM_BACKEND_SUPPORTS_STOP) ?
        api_.backend_stop(backend_) : PRISM_ERROR_NOT_IMPLEMENTED; }
    const char* error(PrismError error) const { return api_.error_string ? api_.error_string(error) : "unavailable"; }
};
}
