// Ports FF1 Menus/BestiaryReader.cs: bestiary (Picture Book) data formatted for speech. Only
// what the game visibly shows - never raw master data, except the dropped and stolen items, which
// the UI shows as icons without text. All six games; stealable items only where the game's
// bestiary lists them (game::flags().bestiary_steal: FF4, FF5, FF6), as FF2-FF6's old mods read
// them.
#include "bestiary.h"
#include "../game.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../text.h"
#include "../unity.h"
#include "../status/status.h"

namespace ffpr::bestiary {
namespace {

ref::Class  kPictureBookData{"Last.Data.PictureBooks", "PictureBookData"};
ref::Method mPbNo{kPictureBookData, "get_No", 0};
ref::Method mPbIsRelease{kPictureBookData, "get_IsRelease", 0};
ref::Method mPbMonsterName{kPictureBookData, "get_MonsterName", 0};

ref::Class  kMonsterData{"Last.OutGame.Library", "MonsterData"};
ref::Method mMonsterHabitatNames{kMonsterData, "get_HabitatNameList", 0};
ref::Method mMonsterMaster{kMonsterData, "get_MonsterMaster", 0};

ref::Class  kInfoContent{"Last.UI.Common.Library", "LibraryInfoContent"};
ref::Field  fMonsterDataTable{kInfoContent, "monsterDataTable"};
ref::Field  fStatusTable{kInfoContent, "statusTable"};
ref::Field  fOptionTable{kInfoContent, "optionTable"};
ref::Field  fHierarchyTable{kInfoContent, "hierarchyTable"};

ref::Class  kParamValue{"Last.UI.Common.Library", "LibraryInfoParamValue"};
ref::Field  fParamText{kParamValue, "paramText"};
ref::Field  fValueText{kParamValue, "valueText"};
ref::Field  fMultipliedValueText{kParamValue, "multipliedValueText"};
ref::Field  fParameterValueText{kParamValue, "parameterValueText"};
ref::Field  fPercentText{kParamValue, "percentText"};
ref::Field  fPersentValueText{kParamValue, "persentValueText"};
ref::Field  fDefaultValueText{kParamValue, "defaultValueText"};
ref::Field  fMultipliedText{kParamValue, "multipliedText"};

ref::Class  kParamListView{"Last.UI.Common.Library", "LibraryInfoContentParamListView"};
ref::Field  fListTitleText{kParamListView, "titleText"};
ref::Field  fListTextTable{kParamListView, "textTable"};
ref::Class  kParamTextView{"Last.UI.Common.Library", "LibraryInfoParamText"};
ref::Field  fDetailText{kParamTextView, "detailText"};

ref::Class  kMonsterParty{"Last.Data.Master", "MonsterParty"};
ref::Method mParty1{kMonsterParty, "get_Monster1", 0};
ref::Method mParty2{kMonsterParty, "get_Monster2", 0};
ref::Method mParty3{kMonsterParty, "get_Monster3", 0};
ref::Method mParty4{kMonsterParty, "get_Monster4", 0};
ref::Method mParty5{kMonsterParty, "get_Monster5", 0};
ref::Method mParty6{kMonsterParty, "get_Monster6", 0};
ref::Method mParty7{kMonsterParty, "get_Monster7", 0};
ref::Method mParty8{kMonsterParty, "get_Monster8", 0};
ref::Method mParty9{kMonsterParty, "get_Monster9", 0};

ref::Class  kMonster{"Last.Data.Master", "Monster"};
ref::Method mMonsterMesIdName{kMonster, "get_MesIdName", 0};
ref::Method mDrop1{kMonster, "get_DropContentId1", 0};
ref::Method mDrop2{kMonster, "get_DropContentId2", 0};
ref::Method mDrop3{kMonster, "get_DropContentId3", 0};
ref::Method mDrop4{kMonster, "get_DropContentId4", 0};
ref::Method mDrop5{kMonster, "get_DropContentId5", 0};
ref::Method mDrop6{kMonster, "get_DropContentId6", 0};
ref::Method mDrop7{kMonster, "get_DropContentId7", 0};
ref::Method mDrop8{kMonster, "get_DropContentId8", 0};
// Steal slots: read only where game::flags().bestiary_steal, so masked to those games.
constexpr uint8_t kStealGames = game::kFF4 | game::kFF5 | game::kFF6;
ref::Class  kMonsterSteal{"Last.Data.Master", "Monster", kStealGames};
ref::Method mSteal1{kMonsterSteal, "get_StealContentId1", 0};
ref::Method mSteal2{kMonsterSteal, "get_StealContentId2", 0};
ref::Method mSteal3{kMonsterSteal, "get_StealContentId3", 0};
ref::Method mSteal4{kMonsterSteal, "get_StealContentId4", 0};

ref::Class  kContent{"Last.Data.Master", "Content"};
ref::Method mContentMesIdName{kContent, "get_MesIdName", 0};

// C# Text.text?.Trim() of a Text field, empty when missing.
std::wstring trimmed_text(void* owner, ref::Field& f)
{
    void* t = owner ? ref::rd<void*>(owner, f) : nullptr;
    return t ? text::trim(unity::ui_text(t)) : std::wstring();
}

// GetParamValueLabel: paramText.text, trimmed.
std::wstring param_value_label(void* param)
{
    void* t = ref::rd<void*>(param, fParamText);
    if (!t) return {};
    std::wstring s = unity::ui_text(t);
    return s.empty() ? std::wstring() : text::trim(s);
}

// GetParamValueText: the first non-empty of the value text fields, trimmed.
std::wstring param_value_text(void* param)
{
    ref::Field* fields[] = {&fValueText, &fMultipliedValueText, &fParameterValueText, &fPercentText,
                            &fPersentValueText, &fDefaultValueText, &fMultipliedText};
    for (ref::Field* f : fields) {
        void* t = ref::rd<void*>(param, *f);
        if (!t) continue;
        std::wstring s = unity::ui_text(t);
        if (!s.empty()) return text::trim(s);
    }
    return {};
}

// ReadParamValueArray: LibraryInfoParamValue[] (label: value pairs), active entries only.
void read_param_value_array(void* table, StatGroup group, std::vector<StatEntry>& entries)
{
    int n = unity::array_length(table);
    for (int i = 0; i < n; ++i) {
        void* param = unity::array_at(table, i);
        if (!param || !unity::active_in_hierarchy(param)) continue;
        std::wstring label = param_value_label(param);
        std::wstring value = param_value_text(param);
        if (!label.empty()) entries.push_back({label, value, group});
    }
}

// ReadParamListViewArray: LibraryInfoContentParamListView[] (title + active detail texts).
void read_param_list_view_array(void* table, StatGroup group, std::vector<StatEntry>& entries)
{
    int n = unity::array_length(table);
    for (int i = 0; i < n; ++i) {
        void* list_view = unity::array_at(table, i);
        if (!list_view || !unity::active_in_hierarchy(list_view)) continue;
        std::wstring title = trimmed_text(list_view, fListTitleText);
        if (title.empty()) continue;

        std::vector<std::wstring> details;
        void* text_table = ref::rd<void*>(list_view, fListTextTable);
        int m = unity::array_length(text_table);
        for (int j = 0; j < m; ++j) {
            void* param_text = unity::array_at(text_table, j);
            if (!param_text || !unity::active_in_hierarchy(param_text)) continue;
            std::wstring detail = trimmed_text(param_text, fDetailText);
            if (!detail.empty()) details.push_back(detail);
        }

        std::wstring value;
        for (size_t k = 0; k < details.size(); ++k) {
            if (k) value += L", ";
            value += details[k];
        }
        entries.push_back({title, details.empty() ? T(L"None") : value, group});
    }
}

// GetGameMessage: MessageManager.GetMessage(id) (isReplace true), empty when missing.
std::wstring game_message(const std::wstring& mes_id)
{
    if (mes_id.empty()) return {};
    return ffpr::status::game_message(mes_id, true);
}

std::wstring monster_fallback(int monster_id) { return TF(L"Monster {0}", {text::num(monster_id)}); }

// ResolveMonsterName: the monster master's name through the game's localization.
std::wstring resolve_monster_name(int monster_id)
{
    void* monster = ffpr::status::master_entry(kMonster, monster_id);
    if (!monster) return monster_fallback(monster_id);
    std::wstring mes_id = ref::call_str(mMonsterMesIdName, monster);
    if (mes_id.empty()) return monster_fallback(monster_id);
    std::wstring name = game_message(mes_id);
    return name.empty() ? monster_fallback(monster_id) : name;
}

// ResolveContentName: an item (Content) name, icon markup stripped.
std::wstring resolve_content_name(int content_id)
{
    void* content = ffpr::status::master_entry(kContent, content_id);
    if (!content) return {};
    std::wstring mes_id = ref::call_str(mContentMesIdName, content);
    if (mes_id.empty()) return {};
    return text::trim(text::strip_markup(game_message(mes_id)));
}

// The item names in a list of Monster master slots, ", "-joined.
template <size_t N>
std::wstring item_names(void* master, ref::Method* (&slots)[N])
{
    std::wstring names;
    for (ref::Method* m : slots) {
        int id = ref::call<int32_t>(*m, master);
        if (id <= 0) continue;
        std::wstring name = resolve_content_name(id);
        if (name.empty()) continue;
        if (!names.empty()) names += L", ";
        names += name;
    }
    return names;
}

// ReadItemsFromMasterData: the 4 steal slots (games whose bestiary lists them), then the 8 drop
// slots of the Monster master.
void read_items_from_master_data(void* monster_data, std::vector<StatEntry>& entries)
{
    if (!monster_data) return;
    void* master = ref::call<void*>(mMonsterMaster, monster_data);
    if (!master) return;
    if (game::flags().bestiary_steal) {
        ref::Method* steals[] = {&mSteal1, &mSteal2, &mSteal3, &mSteal4};
        std::wstring names = item_names(master, steals);
        entries.push_back({T(L"Stealable Items"), names.empty() ? T(L"None") : names, StatGroup::Items});
    }
    ref::Method* drops[] = {&mDrop1, &mDrop2, &mDrop3, &mDrop4, &mDrop5, &mDrop6, &mDrop7, &mDrop8};
    std::wstring names = item_names(master, drops);
    entries.push_back({T(L"Dropped Items"), names.empty() ? T(L"None") : names, StatGroup::Items});
}

} // namespace

std::wstring read_list_entry(void* data)
{
    if (!data) return {};
    // No.ToString("D3")
    int no = ref::call<int32_t>(mPbNo, data);
    std::wstring digits = text::num(no < 0 ? -static_cast<long long>(no) : no);
    while (digits.size() < 3) digits.insert(digits.begin(), L'0');
    std::wstring number = (no < 0 ? L"-" : L"") + digits;
    std::wstring name = ref::call<bool>(mPbIsRelease, data) ? ref::call_str(mPbMonsterName, data) : std::wstring(L"???");
    return number + L": " + name;
}

std::wstring read_encounter_summary(void* list)
{
    if (!list) return {};
    int total = unity::list_count(list);
    int encountered = 0;
    for (int i = 0; i < total; ++i) {
        void* item = unity::list_at(list, i);
        if (item && ref::call<bool>(mPbIsRelease, item)) ++encountered;
    }
    return TF(L"Bestiary. Encountered: {0} of {1}", {text::num(encountered), text::num(total)});
}

std::vector<StatEntry> build_stat_buffer(void* content, void* monster_data)
{
    std::vector<StatEntry> entries;
    if (!content) return entries;
    try {
        // Group 1: Monster Data (Defeated, Level)
        read_param_value_array(ref::rd<void*>(content, fMonsterDataTable), StatGroup::MonsterData, entries);
        // Group 2: Status (HP, MP, Attack, Defense, ...)
        read_param_value_array(ref::rd<void*>(content, fStatusTable), StatGroup::Status, entries);
        // Group 3: Options (Gil, EXP)
        read_param_value_array(ref::rd<void*>(content, fOptionTable), StatGroup::Options, entries);
        // Group 4: Items, from master data (the UI shows icons without text)
        read_items_from_master_data(monster_data, entries);
        // Group 5: Hierarchy (Type, Weakness, Resistance, Absorbs, Cancels, Habitat)
        read_param_list_view_array(ref::rd<void*>(content, fHierarchyTable), StatGroup::Properties, entries);
    } catch (...) {
        LOGF("[Bestiary] Error building stat buffer");
    }
    return entries;
}

std::wstring read_formation(int formation_index, void* party)
{
    if (!party) return {};
    try {
        ref::Method* slots[] = {&mParty1, &mParty2, &mParty3, &mParty4, &mParty5, &mParty6, &mParty7, &mParty8, &mParty9};
        std::wstring names;
        for (ref::Method* m : slots) {
            int id = ref::call<int32_t>(*m, party);
            if (id <= 0) continue;
            std::wstring name = resolve_monster_name(id);
            if (name.empty()) continue;
            if (!names.empty()) names += L", ";
            names += name;
        }
        if (names.empty()) return TF(L"Formation {0}: {1}", {text::num(formation_index + 1), T(L"Empty")});
        return TF(L"Formation {0}: {1}", {text::num(formation_index + 1), names});
    } catch (...) {
        LOGF("[Bestiary] Error reading formation");
        return TF(L"Formation {0}: {1}", {text::num(formation_index + 1), T(L"Error")});
    }
}

std::wstring read_map_name(void* data, int map_index)
{
    if (!data) return {};
    try {
        void* habitat_names = ref::call<void*>(mMonsterHabitatNames, data);
        int count = unity::list_count(habitat_names);
        if (!habitat_names || count == 0) return T(L"No habitat data");
        if (map_index < 0 || map_index >= count) map_index = 0;
        std::wstring name = ref::str(unity::list_at(habitat_names, map_index));
        return !name.empty() ? name : T(L"Unknown location");
    } catch (...) {
        LOGF("[Bestiary] Error reading map name");
        return T(L"Error reading habitat");
    }
}

} // namespace ffpr::bestiary
