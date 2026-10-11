#include "displayed_ui.h"
#include "core.h"
#include "hooks.h"
#include "modtext.h"
#include "unity.h"
#include <map>

namespace displayed {
namespace {
ref::Class kContent{"Last.UI.KeyInput", "EquipmentStatusContentController"};
ref::Class kUpBool{"Last.UI.KeyInput", "EquipmentStatusContentController", game::kFF2 | game::kFF5};
ref::Class kUpVoid{"Last.UI.KeyInput", "EquipmentStatusContentController", game::kFF1 | game::kFF3 | game::kFF4 | game::kFF6};
ref::Method mUp{kUpBool, "SetUpperArrow", 1}, mUpVoid{kUpVoid, "SetUpperArrow", 0};
ref::Method mDown{kUpBool, "SetDownArrow", 1}, mDownVoid{kUpVoid, "SetDownArrow", 0};
ref::Method mDefault{kContent, "SetDefaultArrow", 0};
std::map<void*, int> g_arrows;
using Arrow = void (*)(void*, bool, const void*);
Arrow o_up, o_down;
using Default = void (*)(void*, const void*);
Default o_default;
Default o_up_void;
Default o_down_void;
void record(void* content, int direction)
{
    for (auto it = g_arrows.begin(); it != g_arrows.end();)
        if (!unity::alive(it->first)) it = g_arrows.erase(it); else ++it;
    void* view = field(content, "view");
    if (view) g_arrows[view] = direction;
}
void d_up(void* self, bool rate, const void* mi)
{
    o_up(self, rate, mi);
    hooks::guarded("Displayed.ComparisonUp", [=] { record(self, 1); });
}
void d_up_void(void* self, const void* mi)
{
    o_up_void(self, mi);
    hooks::guarded("Displayed.ComparisonUp", [=] { record(self, 1); });
}
void d_down(void* self, bool rate, const void* mi)
{
    o_down(self, rate, mi);
    hooks::guarded("Displayed.ComparisonDown", [=] { record(self, -1); });
}
void d_down_void(void* self, const void* mi)
{
    o_down_void(self, mi);
    hooks::guarded("Displayed.ComparisonDown", [=] { record(self, -1); });
}
void d_default(void* self, const void* mi)
{
    o_default(self, mi);
    hooks::guarded("Displayed.ComparisonDefault", [=] { record(self, 0); });
}
int install()
{
    int n = hooks::attach(mUp, &d_up, &o_up) ? 1 : 0;
    n += hooks::attach(mUpVoid, &d_up_void, &o_up_void) ? 1 : 0;
    n += hooks::attach(mDown, &d_down, &o_down) ? 1 : 0;
    n += hooks::attach(mDownVoid, &d_down_void, &o_down_void) ? 1 : 0;
    return n + (hooks::attach(mDefault, &d_default, &o_default) ? 1 : 0);
}
core::Install s_install{"Displayed.Comparisons", &install, 105};
}
std::wstring comparison_arrow(void* view)
{
    auto it = g_arrows.find(view);
    if (it == g_arrows.end() || !image_visible(field(view, "arrowImage"))) return {};
    return it->second > 0 ? T(L"up arrow") : it->second < 0 ? T(L"down arrow") : std::wstring();
}
}
