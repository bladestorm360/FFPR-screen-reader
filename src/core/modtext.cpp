#include <windows.h>
#include <map>
#include <atomic>
#include "modtext.h"
#include "game.h"
#include "log.h"
#include "refs.h"
#include "text.h"
#include "frame.h"

namespace data {
namespace {
HMODULE self_module()
{
    HMODULE h = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&self_module), &h);
    return h;
}
using Tables = std::map<std::string, json::Value>;
std::atomic<const Tables*> g_cache{nullptr};
}

std::string resource(const char* name)
{
    HMODULE self = self_module();
    if (!self) return {};
    std::wstring wname = text::widen(name);
    HRSRC r = FindResourceW(self, wname.c_str(), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!r) return {};
    HGLOBAL g = LoadResource(self, r);
    DWORD size = SizeofResource(self, r);
    const void* p = g ? LockResource(g) : nullptr;
    if (!p || !size) return {};
    return std::string(static_cast<const char*>(p), size);
}

std::string game_file(const char* kind)
{
    std::string name;
    for (const char* c = kind; *c; ++c) name.push_back(static_cast<char>(toupper(static_cast<unsigned char>(*c))));
    name += "_";
    for (const char* c = game::key(); *c; ++c) name.push_back(static_cast<char>(toupper(static_cast<unsigned char>(*c))));
    return resource(name.c_str());
}

void prepare_game_json()
{
    if (game_data_ready()) return;
    auto* tables = new Tables;
    for (const char* kind : {"translation", "eventlabels", "maptitles", "mapstate", "puzzles"}) {
        json::Value v;
        const std::string raw = game_file(kind);
        if (!raw.empty()) {
            std::string err;
            if (!json::parse(raw, v, &err)) {
                LOGF("data: %s_%s is malformed: %s", kind, game::key(), err.c_str());
                v = json::Value{};
            }
        }
        tables->emplace(kind, std::move(v));
    }
    g_cache.store(tables, std::memory_order_release);
}
bool game_data_ready() { return g_cache.load(std::memory_order_acquire) != nullptr; }
const json::Value& game_json(const char* kind)
{
    static const json::Value empty;
    const auto* tables = g_cache.load(std::memory_order_acquire);
    if (!tables) return empty;
    const auto it = tables->find(kind);
    return it == tables->end() ? empty : it->second;
}

} // namespace data

namespace modtext {
namespace {

json::Value g_common;
json::Value g_game;
std::wstring g_lang = L"en";
bool g_lang_known = false;
thread_local std::optional<std::wstring> g_worker_language;

ref::Class kMessageManager{"Last.Management", "MessageManager"};
ref::Field fCurrentLanguage{kMessageManager, "currentLanguage"};

const wchar_t* code_for(int id)
{
    static const wchar_t* codes[] = { nullptr, L"ja", L"en", L"fr", L"it", L"de", L"es",
                                      L"ko", L"zht", L"zhc", L"ru", L"th", L"pt" };
    return (id >= 1 && id <= 12) ? codes[id] : nullptr;
}

const std::wstring* pick(const json::Value& file, const std::wstring& key)
{
    const json::Value* entry = file.get(key);
    if (!entry || !entry->is_object()) return nullptr;
    const auto& language = g_worker_language ? *g_worker_language : g_lang;
    const json::Value* v = entry->get(language);
    if (v && v->is_string() && !v->s.empty()) return &v->s;
    if (language != L"en") {
        v = entry->get(L"en");
        if (v && v->is_string() && !v->s.empty()) return &v->s;
    }
    return nullptr;
}

} // namespace

void init()
{
    std::string err;
    std::string common = data::resource("MODTEXT_COMMON");
    if (common.empty() || !json::parse(common, g_common, &err))
        LOGF("modtext: common mod_text missing or malformed %s", err.c_str());
    std::string game = data::game_file("modtext");
    if (!game.empty() && !json::parse(game, g_game, &err))
        LOGF("modtext: %s overrides malformed: %s", game::key(), err.c_str());
    LOGF("modtext: %zu common keys, %zu %s overrides", g_common.obj.size(), g_game.obj.size(), game::key());
}
LocaleScope::LocaleScope(std::wstring language) : previous_(std::move(g_worker_language)) {
    g_worker_language = std::move(language);
}
LocaleScope::~LocaleScope() { g_worker_language = std::move(previous_); }

std::wstring T(std::wstring_view key)
{
    if (!g_worker_language && !g_lang_known && frame::on_main_thread()) refresh_language();
    std::wstring k(key);
    if (const std::wstring* s = pick(g_game, k)) return *s;
    if (const std::wstring* s = pick(g_common, k)) return *s;
    return k;
}

std::wstring TF(std::wstring_view key, std::initializer_list<std::wstring> args)
{
    return text::format(T(key), args);
}

bool has(std::wstring_view key)
{
    std::wstring k(key);
    return g_game.get(k) || g_common.get(k);
}

const std::wstring& lang() { return g_worker_language ? *g_worker_language : g_lang; }

void refresh_language()
{
    // Only from the main thread: the instance getter runs game code.
    if (!frame::on_main_thread()) return;
    void* mm = ref::instance(kMessageManager);
    if (!mm) return;
    int id = ref::rd<int>(mm, fCurrentLanguage);
    if (const wchar_t* code = code_for(id)) {
        if (!g_lang_known || g_lang != code) LOGF("modtext: language %d", id);
        g_lang = code;
        g_lang_known = true;
    }
}

} // namespace modtext
