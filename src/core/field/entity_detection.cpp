// Ports FF1 Field/FieldEntityState.cs and Field/EntityDetectionHelpers.cs.
//
// Reflection in the C# mod resolved members on the STATIC wrapper type Il2CppInterop hands out:
// FieldEntity.Property returns a PropertyEntity wrapper, so GetTreasureContents' lookups
// ("GilAmount", "ContentId", "ItemId", ...) never found a member there, while
// GetInteractiveObjectName's "ObjectId" and GetObjectType's "ObjectType" did. The port reads
// the same members of PropertyEntity, so it says what FF1 says.
#include <cwctype>
#include <set>
#include "field_internal.h"
#include "entity_translator.h"
#include "map_names.h"
#include "../hooks.h"
#include "../il2cpp.h"
#include "../log.h"
#include "../modtext.h"
#include "../text.h"

namespace field {

// =========================================================================================
// FieldEntityState
// =========================================================================================
namespace {
// FieldController.ChangeTransportationSwitchEntity caches with FieldEntity.CacheActive(4)
constexpr int kVehicleSwitchCacheBit = 1 << 4;
// PropertyEvent.ActionId in the map data (Tiled action_id): 0 = nothing happens
constexpr int kActionNone = 0;

// Il2Cpp.MapConstants.ObjectType (identical in all five games; RandomEvent exists in FF5 only)
constexpr int kObjEvent = 2;
constexpr int kObjEntity = 5;
constexpr int kObjAnimEntity = 10;
constexpr int kObjTransportationEventAction = 21;
constexpr int kObjRandomEvent = 28;
} // namespace

bool entity_is_present(void* fe) { return entity_is_active(fe) || entity_is_hidden_by_vehicle(fe); }

bool entity_is_active(void* fe)
{
    if (!live_entity(fe)) return false;
    return unity::active_in_hierarchy(fe);
}

// FieldController.ChangeTransportationSwitchEntity runs for every entity on each
// SetEventEntityGroup and ChangeTransportation. For an entity whose
// Property.TargetTransportationIdList does not hold the current transportation it calls
// RestoreCacheActive(4), CacheActive(4) and Hide(0) (GameObject.SetActive(false)). So
// vehicle-only triggers are inactive whenever the player is on foot or in another vehicle.
// True when the entity is inactive for that reason alone.
bool entity_is_hidden_by_vehicle(void* fe)
{
    if (!live_entity(fe)) return false;
    void* go = unity::game_object(fe);
    if (!go || unity::active_self(go)) return false;
    void* tr = unity::transform(fe);
    void* parent = tr ? unity::parent(tr) : nullptr;
    if (parent && !unity::active_in_hierarchy(parent)) return false;
    void* property = entity_property(fe);
    if (!property) return false;
    void* ids = ref::rd<void*>(property, r::fPropTargetTransportationIdList);
    if (!ids || unity::list_count(ids) == 0) return false;
    int enable = ref::rd<int32_t>(fe, r::fCacheActiveEnable);
    int flag = ref::rd<int32_t>(fe, r::fCacheActiveFlag);
    return (enable & kVehicleSwitchCacheBit) != 0 && (flag & kVehicleSwitchCacheBit) != 0;
}

// An event, map object or vehicle trigger that does nothing when the player checks or touches
// it: no action, no script, no message. These are scenery and cutscene markers and are not
// listed. Anything with an action, a script or a message stays.
bool entity_is_scenery(void* fe)
{
    if (!live_entity(fe)) return false;
    void* property = entity_property(fe);
    if (!property) return false;
    switch (ref::rd<int32_t>(property, r::fPropObjectType)) {
    case kObjEvent:
    case kObjEntity:
    case kObjAnimEntity:
    case kObjTransportationEventAction:
    case kObjRandomEvent:
        break;
    default:
        return false;
    }
    if (r::kPropertyTransportation.is(property)) return false;   // a vehicle's own map object
    if (!r::kPropertyEvent.is(property)) return false;
    if (ref::rd<int32_t>(property, r::fEventActionId) != kActionNone) return false;
    if (ref::rd<int32_t>(property, r::fEventScriptId) != 0) return false;
    if (!r::kPropertyTalk.is(property)) return true;
    return ref::rd_str(property, r::fTalkMessageKey).empty();
}

// =========================================================================================
// EntityDetectionHelpers
// =========================================================================================
namespace {

bool starts_with_ci(const std::wstring& s, const wchar_t* prefix)
{
    std::wstring p(prefix);
    if (s.size() < p.size()) return false;
    for (size_t i = 0; i < p.size(); ++i)
        if (std::towlower(s[i]) != std::towlower(p[i])) return false;
    return true;
}

// Keyword -> display class. Order matters: more-specific keywords first so they win.
struct Keyword { const wchar_t* keyword; const wchar_t* label; };
const Keyword kDialogueClassKeywords[] = {
    // English keywords - more specific multi-word forms before single-word generics
    {L"grave marker", L"Grave Marker"},
    {L"gravestone", L"Gravestone"},
    {L"tombstone", L"Tombstone"},
    {L"tomb", L"Tomb"},
    {L"signpost", L"Sign"},
    {L"sign", L"Sign"},
    {L"bookcase", L"Bookcase"},
    {L"bookshelf", L"Bookshelf"},
    {L"altar", L"Altar"},
    {L"statue", L"Statue"},
    {L"throne", L"Throne"},
    {L"boulder", L"Boulder"},
    {L"rock", L"Rock"},
    {L"crystal", L"Crystal"},
    {L"orb", L"Orb"},
    {L"pedestal", L"Pedestal"},
    {L"plate", L"Plate"},
    {L"barrel", L"Barrel"},
    {L"crate", L"Crate"},
    {L"pot", L"Pot"},
    {L"vase", L"Vase"},
    {L"jar", L"Jar"},
    {L"bed", L"Bed"},
    {L"table", L"Table"},
    {L"chair", L"Chair"},
    {L"piano", L"Piano"},
    {L"painting", L"Painting"},
    {L"mirror", L"Mirror"},
    {L"clock", L"Clock"},
    {L"lever", L"Lever"},
    {L"switch", L"Switch"},
    {L"button", L"Button"},
    {L"chest", L"Chest"},
    {L"gate", L"Gate"},
    {L"ladder", L"Ladder"},
    {L"stairs", L"Stairs"},
    {L"well", L"Well"},
    {L"fountain", L"Fountain"},
    // Japanese keywords - for players playing in Japanese where dialogue is in JA.
    {L"墓標", L"Grave Marker"},                 // 墓標
    {L"墓石", L"Gravestone"},                   // 墓石
    {L"看板", L"Sign"},                         // 看板
    {L"立て札", L"Sign"},                   // 立て札
    {L"本棚", L"Bookshelf"},                    // 本棚
    {L"祭壇", L"Altar"},                        // 祭壇
    {L"像", L"Statue"},                             // 像
    {L"玉座", L"Throne"},                       // 玉座
    {L"岩", L"Rock"},                               // 岩
    {L"クリスタル", L"Crystal"},    // クリスタル
    {L"井戸", L"Well"},                         // 井戸
    {L"噴水", L"Fountain"},                     // 噴水
    {L"樽", L"Barrel"},                             // 樽
    {L"壺", L"Pot"},                                // 壺
    {L"ベッド", L"Bed"},                    // ベッド
};

// Spoken form of a dialogue-classifier label: the table keeps English labels; each is the
// mod_text key of its localized form (LocalizeObjectClass).
std::wstring localize_object_class(const std::wstring& label)
{
    return T(label);
}

std::set<std::wstring> g_logged_unclassified_keys;

// .NET ToLowerInvariant for the classifier match.
std::wstring lower_invariant(const std::wstring& s) { return text::lower(s); }

} // namespace

/// EntityDetectionHelpers.GetObjectType: PropertyEntity.ObjectType, or -1.
int object_type_of(void* property)
{
    if (!property) return -1;
    if (r::fPropObjectType.get() < 0) return -1;
    return ref::rd<int32_t>(property, r::fPropObjectType);
}

/// GetGotoMapDestinationId: PropertyGotoMap.MapId, else PropertyEntity.TmeId, when > 0.
int goto_map_destination_id(void* fe)
{
    void* property = entity_property(fe);
    if (!property) return -1;
    if (r::kPropertyGotoMap.is(property)) {
        int map_id = ref::rd<int32_t>(property, r::fGotoMapId);
        if (map_id > 0) return map_id;
    } else {
        int tme_id = ref::rd<int32_t>(property, r::fPropTmeId);
        if (tme_id > 0) return tme_id;
    }
    return -1;
}

/// ResolveMapName: the destination's localized name ("" for no destination).
std::wstring resolve_map_name(int dest_map_id)
{
    if (dest_map_id <= 0) return L"";
    return hooks::guarded_r("Field.ResolveMapName", TF(L"Map {0}", {text::num(dest_map_id)}),
                            [&] { return map_exit_name(dest_map_id); });
}

/// GetTreasureContents (see the file comment: FF1 finds none of these members on the
/// PropertyEntity wrapper, so the chest is named "Treasure Chest").
std::wstring treasure_contents(void* property)
{
    if (!property) return L"";
    // Explicit opt-in path. The actual derived chest property has ContentId;
    // the historical base PropertyEntity reflection below never found it.
    static ref::Class chest{"Last.Map", "PropertyTresureBox"};
    static ref::Field content{chest, "<ContentId>k__BackingField"};
    if (chest.is(property)) return resolve_item_name(ref::rd<int32_t>(property, content));
    // GilAmount / ContentId / ItemId / ContentsId / RewardId / itemId are not members of
    // PropertyEntity in any of the five games, which is the type FF1's reflection searched.
    il2cpp::Class* k = r::kPropertyEntity.get();
    if (!k) return L"";
    int32_t gil_off = il2cpp::field_offset(k, "<GilAmount>k__BackingField");
    if (gil_off >= 0) {
        int gil = il2cpp::read_field<int32_t>(property, gil_off);
        if (gil > 0) return TF(L"{0} Gil", {text::num(gil)});
    }
    static const char* const kNames[] = {
        "<ContentId>k__BackingField", "<ItemId>k__BackingField", "<ContentsId>k__BackingField",
        "<RewardId>k__BackingField", "<itemId>k__BackingField",
    };
    for (const char* name : kNames) {
        int32_t off = il2cpp::field_offset(k, name);
        if (off < 0) continue;
        int content_id = il2cpp::read_field<int32_t>(property, off);
        if (content_id > 0) {
            std::wstring item = resolve_item_name(content_id);
            if (!item.empty()) return item;
            return TF(L"Item {0}", {text::num(content_id)});
        }
    }
    return L"";
}

/// ResolveItemName: ContentUtitlity.GetMesIdItemName + MessageManager, markup stripped.
std::wstring resolve_item_name(int content_id)
{
    if (content_id <= 0) return L"";
    std::wstring mes_id = mes_id_item_name(content_id);
    if (mes_id.empty()) return L"";
    std::wstring name = get_message(mes_id);
    if (name.empty()) return L"";
    return text::trim(text::strip_markup(name));
}

/// GetEntityNameFromProperty: PropertyEntity.Name, resolved through MessageManager for
/// mes_/sys_/field_ keys, translated by EntityTranslator. Empty when there is none.
std::wstring entity_name_from_property(void* fe)
{
    void* property = entity_property(fe);
    if (!property) return L"";
    std::wstring name = ref::rd_str(property, r::fPropName);
    if (text::is_blank(name)) return L"";

    if (starts_with_ci(name, L"mes_") || starts_with_ci(name, L"sys_") || starts_with_ci(name, L"field_")) {
        std::wstring localized = get_message(name);
        if (!text::is_blank(localized) && localized != name)
            return translate_entity_name(localized);
    }

    std::wstring lower = text::lower(name);
    if (lower == L"event" || lower == L"eventtrigger" || lower == L"event trigger" || lower == L"pointin")
        return L"";

    return translate_entity_name(name);
}

/// CleanObjectName: "(Clone)" removed, trimmed, numeric/underscore/too-short names replaced by
/// the default, then translated.
std::wstring clean_object_name(const std::wstring& raw, const std::wstring& default_name)
{
    if (text::is_blank(raw)) return default_name;
    std::wstring name = text::trim(text::replace_all(raw, L"(Clone)", L""));

    bool all_digits = true;
    for (wchar_t c : name)
        if (!(is_decimal_digit(c) || c == L'_')) { all_digits = false; break; }
    if (name.size() < 2 || all_digits) return default_name;
    if (!name.empty() && name[0] == L'_') return default_name;

    return translate_entity_name(name);
}

/// ClassifyByDialogue: the examined object's dialogue matched against the keyword table.
/// Empty when nothing matched; an unmatched key is logged once.
std::wstring classify_by_dialogue(void* fe)
{
    void* property = entity_property(fe);
    if (!property) return L"";

    // PropertyDefaultEntity inherits PropertyTalk -> MessageKey through it.
    if (!r::kPropertyDefaultEntity.is(property)) return L"";
    std::wstring message_key = ref::rd_str(property, r::fTalkMessageKey);
    if (message_key.empty()) return L"";

    if (!ref::instance(r::kMessageManager)) return L"";
    std::wstring msg = get_message(message_key);
    if (text::is_blank(msg) || msg == message_key) return L"";

    std::wstring normalized = lower_invariant(text::trim(text::strip_markup(msg)));
    for (const Keyword& kw : kDialogueClassKeywords)
        if (normalized.find(kw.keyword) != std::wstring::npos) return localize_object_class(kw.label);

    if (g_logged_unclassified_keys.insert(message_key).second) {
        std::wstring snippet = text::trim(text::replace_all(msg, L"\n", L" "));
        if (snippet.size() > 80) snippet = snippet.substr(0, 80) + L"…";
        LOGF("[Classify] unmatched key=%ls text=\"%ls\"", message_key.c_str(), snippet.c_str());
    }
    return L"";
}

/// GetInteractiveObjectName: "Object {ObjectId}", else the cleaned object name, else
/// "Interactive Object".
std::wstring interactive_object_name(void* property, const std::wstring& go_name)
{
    if (property) {
        int object_id = ref::rd<int32_t>(property, r::fPropObjectId);
        if (object_id > 0) return TF(L"Object {0}", {text::num(object_id)});
    }
    std::wstring cleaned = clean_object_name(go_name, L"");
    if (!cleaned.empty() && cleaned != L"FieldMapObjectDefault") return cleaned;
    return T(L"Interactive Object");
}

/// CheckIfTreasureOpened: FieldTresureBox.isOpen.
bool treasure_opened(void* fe)
{
    if (!live_entity(fe)) return false;
    if (r::kFieldTresureBox.is(fe)) return ref::rd<bool>(fe, r::fTresureIsOpen);
    return false;   // FieldEntity has no isOpened / isOpen member
}

/// GetVehicleNameFromProperty.
std::wstring vehicle_name_from_property(const std::wstring& go_name, const std::wstring& type_name)
{
    std::wstring lower = text::lower(go_name);
    if (lower.find(L"airship") != std::wstring::npos || type_name.find(L"AirShip") != std::wstring::npos)
        return T(L"Airship");
    if (lower.find(L"canoe") != std::wstring::npos) return T(L"Canoe");
    if (lower.find(L"ship") != std::wstring::npos || lower.find(L"boat") != std::wstring::npos) return T(L"Ship");
    return T(L"Vehicle");
}

/// GetVehicleTypeFromName: TransportationType from the (localized) vehicle name; 0 = unknown.
int vehicle_type_from_name(const std::wstring& vehicle_name)
{
    std::wstring lower = text::lower(vehicle_name);
    if (lower.find(L"airship") != std::wstring::npos) return 3;
    if (lower.find(L"canoe") != std::wstring::npos) return 5;
    if (lower.find(L"ship") != std::wstring::npos || lower.find(L"boat") != std::wstring::npos) return 2;
    return 0;
}

/// IsEntityOnCurrentMap: the first "map_" ancestor (10 levels) must contain the map's asset.
bool entity_on_current_map(void* fe, const std::wstring& current_map_asset)
{
    if (!fe || current_map_asset.empty()) return true;
    return hooks::guarded_r("Field.IsEntityOnCurrentMap", true, [&] {
        void* tr = unity::transform(fe);
        if (!tr) return true;
        void* parent = unity::parent(tr);
        int depth = 0;
        std::wstring asset_lower = text::lower(current_map_asset);
        while (parent && depth < 10) {
            std::wstring parent_lower = text::lower(unity::name(parent));
            if (text::starts_with(parent_lower, L"map_"))
                return parent_lower.find(asset_lower) != std::wstring::npos;
            parent = unity::parent(parent);
            depth++;
        }
        return true;
    });
}

} // namespace field
