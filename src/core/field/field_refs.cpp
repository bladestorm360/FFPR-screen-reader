// Game names used by the field area (the C# mod's Il2CppLast.* / Il2Cpp.* types and the
// IL2CppOffsets of FieldNavigationHelper), resolved by name in every game.
// Ports the type access of FF1 Field/*.cs, Core/EntityNavigationManager.cs and
// Utils/EntityTranslator.cs.
#include <windows.h>
#include "field_internal.h"
#include "../il2cpp.h"
#include "../log.h"

namespace field {
namespace r {

ref::Class kFieldEntity{"Last.Entity.Field", "FieldEntity"};
ref::Field fEntityProperty{kFieldEntity, "<Property>k__BackingField"};
ref::Field fCacheActiveEnable{kFieldEntity, "cacheActiveEnable"};
ref::Field fCacheActiveFlag{kFieldEntity, "cacheActiveFlag"};
ref::Field fIsOnCollision{kFieldEntity, "<IsOnCollision>k__BackingField"};

ref::Class kFieldPlayer{"Last.Entity.Field", "FieldPlayer"};
ref::Class kFieldPlayerController{"Last.Map", "FieldPlayerController"};
ref::Field fFieldPlayer{kFieldPlayerController, "fieldPlayer"};
ref::Field fMapHandle{kFieldPlayerController, "mapHandle"};

ref::Class kFieldMap{"", "FieldMap"};
ref::Field fFieldController{kFieldMap, "fieldController"};

ref::Class kFieldController{"Last.Map", "FieldController"};
ref::Field fEntityList{kFieldController, "entityList"};
ref::Field fColliderEntityList{kFieldController, "colliderEntityList"};
ref::Field fTransportation{kFieldController, "transportation"};
ref::Field fFootEvent{kFieldController, "footEvent"};
ref::Field fMapManager{kFieldController, "mapManager"};
ref::Method mIsCanMoveToDestPosition{kFieldController, "IsCanMoveToDestPosition", 2};

ref::Class kIMapAccessor{"Last.Map", "IMapAccessor"};
ref::Method mGetCollisionLayerWidth{kIMapAccessor, "GetCollisionLayerWidth", 0};
ref::Method mGetCollisionLayerHeight{kIMapAccessor, "GetCollisionLayerHeight", 0};

ref::Class kTransportationController{"Last.Map", "TransportationController"};
ref::Field fInfoData{kTransportationController, "infoData"};
ref::Method mNeedInteractiveList{kTransportationController, "NeedInteractiveList", 0};
ref::Method mGetCurrentTransportation{kTransportationController, "get_CurrentTransportation", 0};
ref::Class kTransportation{"Last.Map", "Transportation"};
ref::Field fModelList{kTransportation, "modelList"};
ref::Class kTransportationInfo{"Last.Map", "TransportationInfo"};
ref::Method mInfoGetEnable{kTransportationInfo, "get_Enable", 0};
ref::Method mInfoGetType{kTransportationInfo, "get_Type", 0};
ref::Method mInfoGetId{kTransportationInfo, "get_Id", 0};
ref::Method mInfoGetMapObject{kTransportationInfo, "get_MapObject", 0};
ref::Method mInfoGetMessageId{kTransportationInfo, "get_MessageId", 0};

ref::Class kFootEvent{"Last.Map", "FootEvent"};
ref::Field fStepOnTriggerList{kFootEvent, "stepOnTriggerList"};

ref::Class kEventTriggerEntity{"Last.Entity.Field", "EventTriggerEntity"};
ref::Class kGotoMapEventEntity{"Last.Entity.Field", "GotoMapEventEntity"};
ref::Class kSwitchLayerEventEntity{"Last.Entity.Field", "SwitchLayerEventEntity"};
ref::Class kFieldTresureBox{"Last.Entity.Field", "FieldTresureBox"};
ref::Field fTresureIsOpen{kFieldTresureBox, "isOpen"};
ref::Class kFieldNonPlayer{"Last.Entity.Field", "FieldNonPlayer"};
ref::Method mNonPlayerGetCanAction{kFieldNonPlayer, "get_CanAction", 0};
ref::Class kFieldMapObjectDefault{"Last.Entity.Field", "FieldMapObjectDefault"};
ref::Method mMapObjectGetCanAction{kFieldMapObjectDefault, "get_CanAction", 0};
ref::Class kIInteractiveEntity{"Last.Entity.Field", "IInteractiveEntity"};

ref::Class kPropertyEntity{"Last.Map", "PropertyEntity"};
ref::Field fPropTmeId{kPropertyEntity, "<TmeId>k__BackingField"};
ref::Field fPropObjectId{kPropertyEntity, "<ObjectId>k__BackingField"};
ref::Field fPropObjectType{kPropertyEntity, "<ObjectType>k__BackingField"};
ref::Field fPropEntityId{kPropertyEntity, "<EntityId>k__BackingField"};
ref::Field fPropName{kPropertyEntity, "<Name>k__BackingField"};
ref::Field fPropTargetTransportationIdList{kPropertyEntity, "<TargetTransportationIdList>k__BackingField"};
ref::Class kPropertyGotoMap{"Last.Map", "PropertyGotoMap"};
ref::Field fGotoMapId{kPropertyGotoMap, "<MapId>k__BackingField"};
ref::Class kPropertyEvent{"Last.Map", "PropertyEvent"};
ref::Field fEventActionId{kPropertyEvent, "<ActionId>k__BackingField"};
ref::Field fEventScriptId{kPropertyEvent, "<ScriptId>k__BackingField"};
ref::Class kPropertyTalk{"", "PropertyTalk"};
ref::Field fTalkMessageKey{kPropertyTalk, "<MessageKey>k__BackingField"};
ref::Class kPropertyDefaultEntity{"Last.Map", "PropertyDefaultEntity"};
ref::Class kPropertyTelepoPoint{"Last.Map", "PropertyTelepoPoint"};
ref::Class kPropertyTransportation{"Last.Map", "PropertyTransportation"};

ref::Class kFieldMapProvisionInformation{"Last.Map", "FieldMapProvisionInformation"};
ref::Method mProvisionGetInstance{kFieldMapProvisionInformation, "get_Instance", 0};
ref::Field fProvisionCurrentMapId{kFieldMapProvisionInformation, "<CurrentMapId>k__BackingField"};
ref::Class kUserDataManager{"Last.Management", "UserDataManager"};
ref::Method mUserDataInstance{kUserDataManager, "Instance", 0};
ref::Field fUserDataCurrentMapId{kUserDataManager, "<CurrentMapId>k__BackingField"};

ref::Class kMasterManager{"Last.Data.Master", "MasterManager"};
ref::Field fMasterList{kMasterManager, "masterList"};
ref::Class kMasterMap{"Last.Data.Master", "Map"};
ref::Method mMapGetAreaId{kMasterMap, "get_AreaId", 0};
ref::Method mMapGetMapTitle{kMasterMap, "get_MapTitle", 0};
ref::Method mMapGetFloor{kMasterMap, "get_Floor", 0};
ref::Method mMapGetAssetName{kMasterMap, "get_AssetName", 0};
ref::Class kMasterArea{"Last.Data.Master", "Area"};
ref::Method mAreaGetAreaName{kMasterArea, "get_AreaName", 0};

ref::Class kMessageManager{"Last.Management", "MessageManager"};
ref::Method mGetMessage{kMessageManager, "GetMessage", 2};
ref::Class kContentUtitlity{"Last.Systems", "ContentUtitlity"};
// Declared for the name check; the int overload is resolved in mes_id_item_name().
ref::Method mGetMesIdItemName{kContentUtitlity, "GetMesIdItemName", 1};

ref::Class kMapRouteSearcher{"", "MapRouteSearcher"};
ref::Method mSearch{kMapRouteSearcher, "Search", 4};
ref::Method mSearchSimple{kMapRouteSearcher, "SearchSimple", 3};

ref::Class kMapManager{"Last.Map", "MapManager"};
ref::Field fCurrentMapModel{kMapManager, "currentMapModel"};
ref::Class kMapModel{"Last.Map", "MapModel"};
ref::Field fCurrentMappingData{kMapModel, "currentMappingData"};
ref::Field fCollisionMappingData{kMapModel, "collisionMappingData"};
ref::Field fHiddenPassageMappingData{kMapModel, "hiddenPassageMappingData"};
ref::Method mSetCurrentMappingData{kMapModel, "SetCurrentMappingData", 1};

ref::Class kGameObject{"UnityEngine", "GameObject"};
ref::Method mGetLayer{kGameObject, "get_layer", 0};

} // namespace r

namespace {
ref::Class kInt32{"System", "Int32"};

using IsAssignableFrom_t = bool (*)(void* klass, void* oklass);
IsAssignableFrom_t g_is_assignable = nullptr;
bool g_is_assignable_tried = false;

IsAssignableFrom_t is_assignable_fn()
{
    if (g_is_assignable_tried) return g_is_assignable;
    g_is_assignable_tried = true;
    // The core binding does not expose il2cpp_class_is_assignable_from (interface checks);
    // the field area binds it itself. Il2CppInterop's TryCast uses the same export.
    if (HMODULE ga = GetModuleHandleW(L"GameAssembly.dll"))
        g_is_assignable = reinterpret_cast<IsAssignableFrom_t>(GetProcAddress(ga, "il2cpp_class_is_assignable_from"));
    if (!g_is_assignable) LOGF("field: il2cpp_class_is_assignable_from not exported; interface checks off");
    return g_is_assignable;
}

il2cpp::Method* g_mes_id_item_name = nullptr;
bool g_mes_id_item_name_tried = false;
} // namespace

bool call_virtual_bool(ref::Method& m, void* obj)
{
    il2cpp::Method* base = m.get();
    if (!base || !obj) return false;
    il2cpp::Method* v = il2cpp::virtual_method(obj, base);
    void* fp = il2cpp::method_pointer(v);
    if (!fp) return false;
    try {
        return reinterpret_cast<bool (*)(void*, const void*)>(fp)(obj, v);
    } catch (...) {
        ref::call_failed_note(m.name);
        return false;
    }
}

int call_virtual_int(ref::Method& m, void* obj)
{
    il2cpp::Method* base = m.get();
    if (!base || !obj) return 0;
    il2cpp::Method* v = il2cpp::virtual_method(obj, base);
    void* fp = il2cpp::method_pointer(v);
    if (!fp) return 0;
    try {
        return reinterpret_cast<int32_t (*)(void*, const void*)>(fp)(obj, v);
    } catch (...) {
        ref::call_failed_note(m.name);
        return 0;
    }
}

bool implements(void* obj, ref::Class& iface)
{
    il2cpp::Class* k = iface.get();
    if (!k || !obj) return false;
    il2cpp::Class* own = il2cpp::object_class(obj);
    if (!own) return false;
    IsAssignableFrom_t fn = is_assignable_fn();
    if (!fn) return false;
    __try {
        return fn(k, own);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::wstring mes_id_item_name(int id)
{
    if (!g_mes_id_item_name_tried) {
        g_mes_id_item_name_tried = true;
        il2cpp::Class* k = r::kContentUtitlity.get();
        il2cpp::Class* i32 = kInt32.get();
        if (k && i32) g_mes_id_item_name = il2cpp::find_method_overload_param(k, "GetMesIdItemName", 1, i32, 0);
        if (!g_mes_id_item_name) LOGF("field: ContentUtitlity.GetMesIdItemName(int) not resolved");
    }
    il2cpp::Method* m = g_mes_id_item_name;
    void* fp = m ? il2cpp::method_pointer(m) : nullptr;
    if (!fp) return {};
    try {
        return ref::str(reinterpret_cast<void* (*)(int32_t, const void*)>(fp)(id, m));
    } catch (...) {
        ref::call_failed_note("GetMesIdItemName");
        return {};
    }
}

std::wstring get_message(const std::wstring& key, bool is_replace)
{
    if (key.empty()) return {};
    void* mm = ref::instance(r::kMessageManager);
    if (!mm) return {};
    void* s = ref::new_str(key);
    if (!s) return {};
    return ref::call_str(r::mGetMessage, mm, s, is_replace);
}

int game_object_layer(void* obj)
{
    void* go = unity::game_object(obj);
    return go ? ref::call<int32_t>(r::mGetLayer, go) : 0;
}

void* entity_property(void* field_entity)
{
    return field_entity ? ref::rd<void*>(field_entity, r::fEntityProperty) : nullptr;
}

bool live_entity(void* field_entity)
{
    return field_entity && r::kFieldEntity.is(field_entity) && unity::alive(field_entity);
}

} // namespace field
