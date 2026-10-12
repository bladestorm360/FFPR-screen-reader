#include "displayed_ui.h"
#include "il2cpp.h"
#include "refs.h"
#include "unity.h"
#include "text.h"
#include "modtext.h"
#include <map>
#include <utility>
#include <cmath>

namespace displayed {
namespace {
ref::Class kBehaviour{"UnityEngine", "Behaviour"};
ref::Method mEnabled{kBehaviour, "get_enabled", 0};
ref::Class kImage{"UnityEngine.UI", "Image"};
ref::Field fFill{kImage, "m_FillAmount"};
ref::Field fImageType{kImage, "m_Type"};
ref::Method mSprite{kImage, "get_overrideSprite", 0};
ref::Class kGraphic{"UnityEngine.UI", "Graphic"};
ref::Method mCanvasRenderer{kGraphic, "get_canvasRenderer", 0};
ref::Method mColor{kGraphic, "get_color", 0};
ref::Class kCanvasRenderer{"UnityEngine", "CanvasRenderer"};
ref::Method mCull{kCanvasRenderer, "get_cull", 0};
ref::Method mAlpha{kCanvasRenderer, "GetInheritedAlpha", 0};
ref::Method mOwnAlpha{kCanvasRenderer, "GetAlpha", 0};
ref::Class kRenderer{"UnityEngine", "Renderer"};
ref::Method mRendererEnabled{kRenderer, "get_enabled", 0}, mRendererVisible{kRenderer, "get_isVisible", 0};
ref::Class kAbilityRow{"Last.UI.KeyInput", "AbilityContentController"};
struct Color { float r, g, b, a; };

std::wstring meter(void* view, const char* current, const char* maximum,
                   const char* gauge, const wchar_t* label)
{
    const auto value = text_field(view, current);
    const auto max = text_field(view, maximum);
    if (value.empty() && max.empty()) {
        const auto percentage = bar(field(view, gauge));
        return percentage.empty() ? std::wstring() : T(label) + L" " + percentage;
    }
    std::wstring result = T(label) + L" " + value;
    if (!max.empty()) result += (value.empty() ? L"" : L"/") + max;
    else {
        const auto percentage = bar(field(view, gauge));
        if (!percentage.empty()) result += L", " + T(label) + L" " + percentage;
    }
    return result;
}
}

void* field(void* object, const char* name)
{
    if (!object || !name) return nullptr;
    auto* cls = il2cpp::object_class(static_cast<il2cpp::Object*>(object));
    if (!cls) return nullptr;
    // Live view types differ by game. Cache names per actual class, including misses.
    static std::map<std::pair<il2cpp::Class*, std::string>, int32_t> offsets;
    const auto key = std::make_pair(cls, std::string(name));
    auto it = offsets.find(key);
    if (it == offsets.end()) it = offsets.emplace(key, il2cpp::field_offset(cls, name)).first;
    return it->second < 0 ? nullptr : il2cpp::read_field<void*>(object, it->second);
}

bool visible(void* component)
{
    if (!unity::alive(component) || !unity::active_in_hierarchy(component)) return false;
    if (kBehaviour.is(component) && !ref::call<bool>(mEnabled, component)) return false;
    if (kGraphic.is(component)) {
        if (mColor && ref::call<Color>(mColor, component).a <= 0) return false;
        void* renderer = ref::call<void*>(mCanvasRenderer, component);
        if (renderer && ((mCull && ref::call<bool>(mCull, renderer)) ||
                         (mAlpha && ref::call<float>(mAlpha, renderer) <= 0) ||
                         (mOwnAlpha && ref::call<float>(mOwnAlpha, renderer) <= 0))) return false;
    }
    return true;
}
bool container_visible(void* component)
{
    return unity::alive(component) && unity::active_in_hierarchy(component);
}
bool world_visible(void* entity)
{
    if (!visible(entity)) return false;
    // All six UnityPlayer binaries register this bool(Renderer*) binding, even
    // when IL2CPP strips the managed get_isVisible wrapper (observed in FF2).
    using IsVisible = bool (*)(void*);
    static IsVisible is_visible = reinterpret_cast<IsVisible>(il2cpp::resolve_icall("UnityEngine.Renderer::get_isVisible"));
    for (void* renderer : unity::components_in_children(entity, kRenderer, false)) {
        if (!visible(renderer) || !ref::call<bool>(mRendererEnabled, renderer)) continue;
        if (mRendererVisible ? ref::call<bool>(mRendererVisible, renderer) : is_visible && is_visible(renderer)) return true;
    }
    return false;
}

std::wstring text(void* component)
{
    return visible(component) ? ::text::clean(unity::ui_text(component)) : std::wstring();
}
bool image_visible(void* component)
{
    return kImage.is(component) && visible(component) && unity::alive(ref::call<void*>(mSprite, component));
}
void* image_sprite(void* component) { return image_visible(component) ? ref::call<void*>(mSprite, component) : nullptr; }
std::wstring text_field(void* view, const char* name) { return text(field(view, name)); }

std::vector<std::wstring> texts(void* root)
{
    std::vector<std::wstring> lines;
    if (!container_visible(root)) return lines;
    for (void* t : unity::texts(root, false)) {
        auto line = text(t);
        if (!line.empty()) lines.push_back(std::move(line));
    }
    return lines;
}

std::wstring pair(void* view, const char* label, const char* value)
{
    auto v = text_field(view, value);
    auto l = text_field(view, label);
    return v.empty() ? std::wstring() : (l.empty() ? v : l + L" " + v);
}

std::optional<float> bar_fraction(void* component)
{
    void* image = kImage.is(component) ? component : field(component, "gaugeImage");
    if (!visible(image) || ref::rd<int32_t>(image, fImageType) != 3) return {};
    const float amount = ref::rd<float>(image, fFill);
    return std::isfinite(amount) && amount >= 0 && amount <= 1 ? std::optional<float>(amount) : std::nullopt;
}
std::wstring bar(void* component)
{
    const auto amount = bar_fraction(component);
    return amount ? std::to_wstring(static_cast<int>(std::lround(*amount * 100))) + L"%" : std::wstring();
}
std::vector<std::wstring> vital_lines(void* view)
{
    std::vector<std::wstring> lines;
    for (auto value : {meter(view, "currentHpText", "maxHpText", "hpGauge", L"HP"),
                       meter(view, "currentMpText", "maxMpText", "mpGauge", L"MP")})
        if (!value.empty()) lines.push_back(std::move(value));
    void* charges = field(view, "currentLvMpTextList");
    for (int i = 0; i < unity::list_count(charges); ++i) {
        auto value = text(unity::list_at(charges, i));
        if (!value.empty()) lines.push_back(TF(L"Level {0}", {::text::num(i + 1)}) + L" " + T(L"MP") + L" " + value);
    }
    return lines;
}
std::wstring vitals(void* view) { return ::text::join(vital_lines(view), L", "); }

std::wstring character(void* view)
{
    auto name = text_field(view, "nameText");
    auto values = vitals(view);
    if (!values.empty()) name += (name.empty() ? L"" : L", ") + values;
    return name;
}
std::wstring target(void* content)
{
    if (!visible(content)) return {};
    auto value = character(field(content, "view"));
    auto icons = conditions(content);
    if (!icons.empty()) value += (value.empty() ? L"" : L", ") + icons;
    return value;
}
std::wstring spell_row(void* content)
{
    auto result = ::text::join(texts(content), L" ");
    if (result.empty()) return {};
    void* view = field(unity::get_component_in_parent(content, kAbilityRow), "view");
    auto level = pair(view, "fixLevelText", "currentLevelText");
    auto current = text_field(view, "currentCountText");
    auto maximum = text_field(view, "maxCountText");
    if (!level.empty()) result += L", " + level;
    if (!current.empty()) result += L", " + T(L"MP") + L" " + current +
        text_field(view, "separateCountText") + maximum;
    return result;
}
}
