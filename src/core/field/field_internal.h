#pragma once
// Internal declarations shared by the files of the field area (src/core/field). Not for other
// areas: they use field_api.h, map_names.h, entity_translator.h, object_cache.h and
// scene_events.h. (NavigationTargetTracker and LocationMessageTracker live in src/core/fieldnav.)
#include <memory>
#include <string>
#include <vector>
#include "../refs.h"
#include "../unity.h"

namespace field {

// --- game names (field_refs.cpp) ----------------------------------------------------------
namespace r {
extern ref::Class kFieldEntity;
extern ref::Field fEntityProperty;
extern ref::Field fCacheActiveEnable;
extern ref::Field fCacheActiveFlag;
extern ref::Field fIsOnCollision;

extern ref::Class kFieldPlayer;
extern ref::Class kFieldPlayerController;
extern ref::Field fFieldPlayer;
extern ref::Field fMapHandle;

extern ref::Class kFieldMap;
extern ref::Field fFieldController;

extern ref::Class kFieldController;
extern ref::Field fEntityList;
extern ref::Field fTransportation;
extern ref::Field fFootEvent;
extern ref::Field fMapManager;
extern ref::Method mIsCanMoveToDestPosition;

extern ref::Class kIMapAccessor;
extern ref::Method mGetCollisionLayerWidth;
extern ref::Method mGetCollisionLayerHeight;

extern ref::Class kTransportationController;
extern ref::Field fInfoData;
extern ref::Method mNeedInteractiveList;
extern ref::Class kTransportation;
extern ref::Field fModelList;
extern ref::Class kTransportationInfo;
extern ref::Method mInfoGetEnable;
extern ref::Method mInfoGetType;
extern ref::Method mInfoGetMapObject;
extern ref::Method mInfoGetMessageId;

extern ref::Class kFootEvent;
extern ref::Field fStepOnTriggerList;

extern ref::Class kEventTriggerEntity;
extern ref::Class kGotoMapEventEntity;
extern ref::Class kSwitchLayerEventEntity;
extern ref::Class kFieldTresureBox;
extern ref::Field fTresureIsOpen;
extern ref::Class kFieldNonPlayer;
extern ref::Method mNonPlayerGetCanAction;
extern ref::Class kFieldMapObjectDefault;
extern ref::Method mMapObjectGetCanAction;
extern ref::Class kIInteractiveEntity;

extern ref::Class kPropertyEntity;
extern ref::Field fPropTmeId;
extern ref::Field fPropObjectId;
extern ref::Field fPropObjectType;
extern ref::Field fPropName;
extern ref::Field fPropTargetTransportationIdList;
extern ref::Class kPropertyGotoMap;
extern ref::Field fGotoMapId;
extern ref::Class kPropertyEvent;
extern ref::Field fEventActionId;
extern ref::Field fEventScriptId;
extern ref::Class kPropertyTalk;
extern ref::Field fTalkMessageKey;
extern ref::Class kPropertyDefaultEntity;
extern ref::Class kPropertyTelepoPoint;
extern ref::Class kPropertyTransportation;

extern ref::Class kFieldMapProvisionInformation;
extern ref::Method mProvisionGetInstance;
extern ref::Field fProvisionCurrentMapId;
extern ref::Class kUserDataManager;
extern ref::Method mUserDataInstance;
extern ref::Field fUserDataCurrentMapId;

extern ref::Class kMasterManager;
extern ref::Field fMasterList;
extern ref::Class kMasterMap;
extern ref::Method mMapGetAreaId;
extern ref::Method mMapGetMapTitle;
extern ref::Method mMapGetFloor;
extern ref::Method mMapGetAssetName;
extern ref::Class kMasterArea;
extern ref::Method mAreaGetAreaName;

extern ref::Class kMessageManager;
extern ref::Method mGetMessage;
extern ref::Class kContentUtitlity;
extern ref::Method mGetMesIdItemName;

extern ref::Class kMapRouteSearcher;
extern ref::Method mSearch;
extern ref::Method mSearchSimple;

extern ref::Class kMapManager;
extern ref::Field fCurrentMapModel;
extern ref::Class kMapModel;
extern ref::Field fCurrentMappingData;
extern ref::Field fCollisionMappingData;
extern ref::Field fHiddenPassageMappingData;
extern ref::Method mSetCurrentMappingData;

extern ref::Class kGameObject;
extern ref::Method mGetLayer;
} // namespace r

// --- small call helpers (field_refs.cpp) ----------------------------------------------------
/// The override `obj` runs for a virtual or interface method (C# virtual dispatch).
bool call_virtual_bool(ref::Method& m, void* obj);
int call_virtual_int(ref::Method& m, void* obj);
/// IL2CPP TryCast to an interface: true when obj's class implements `iface`.
bool implements(void* obj, ref::Class& iface);
/// ContentUtitlity.GetMesIdItemName(int) (the int overload; the Item overload has the same argc).
std::wstring mes_id_item_name(int id);
/// MessageManager.Instance.GetMessage(key, is_replace); empty when unavailable. FF1 passes false.
std::wstring get_message(const std::wstring& key, bool is_replace = false);
/// GameObject.layer of a component's game object (0 when unavailable).
int game_object_layer(void* obj);
/// The FieldEntity's PropertyEntity (FieldEntity.Property), or null.
void* entity_property(void* field_entity);
/// A FieldEntity pointer we stored earlier that is still a live FieldEntity.
bool live_entity(void* field_entity);

// --- vehicle data (navigation.cpp) ----------------------------------------------------------
/// FieldNavigationHelper.VehicleTypeMap lookup: the transportation type of a vehicle map
/// object found in Transportation.ModelList, and its message id (FF4/FF5 names).
bool vehicle_type_of(void* field_entity, int& type, std::wstring& message_id);

// --- BreadcrumbRouteChainer (breadcrumb_chain.cpp) --------------------------------------------
/// FF5 BreadcrumbRouteChainer.TryChain: chains MapRouteSearcher searches through reachable
/// midpoints to a target beyond the game's ~31-cell window. False (out_path untouched) when the
/// target is inside the window, there is no player, or no route was found. True with the stitched
/// world-space path from the player to the target.
bool try_breadcrumb_chain(unity::Vec3 player_world_pos, unity::Vec3 target_world_pos, void* map_handle,
                          void* player, std::vector<unity::Vec3>& out_path);

// --- FieldEntityState (entity_detection.cpp) ------------------------------------------------
bool entity_is_present(void* field_entity);
bool entity_is_active(void* field_entity);
bool entity_is_hidden_by_vehicle(void* field_entity);
bool entity_is_scenery(void* field_entity);

// --- EntityDetectionHelpers (entity_detection.cpp) ------------------------------------------
int object_type_of(void* property);
int goto_map_destination_id(void* field_entity);
std::wstring resolve_map_name(int dest_map_id);
std::wstring treasure_contents(void* property);
std::wstring resolve_item_name(int content_id);
std::wstring entity_name_from_property(void* field_entity);
std::wstring clean_object_name(const std::wstring& name, const std::wstring& default_name);
std::wstring classify_by_dialogue(void* field_entity);
std::wstring interactive_object_name(void* property, const std::wstring& go_name);
bool treasure_opened(void* field_entity);
std::wstring vehicle_name_from_property(const std::wstring& go_name, const std::wstring& type_name);
int vehicle_type_from_name(const std::wstring& vehicle_name);
bool entity_on_current_map(void* field_entity, const std::wstring& current_map_asset);

// --- NavigableEntity (navigable_entity.cpp) -------------------------------------------------
enum class Category { All = 0, Chests = 1, NPCs = 2, MapExits = 3, Events = 4, Vehicles = 5 };
constexpr int kCategoryCount = 6;

enum class Kind { TreasureChest, NPC, MapExit, SavePoint, Event, ScriptTrigger, Vehicle };

struct Entity {
    Kind kind = Kind::Event;
    void* game = nullptr;          // the FieldEntity (null for a ScriptTrigger)
    void* game_class = nullptr;    // its class when converted (a reused address is not the same entity)
    unity::Vec3 pos{};             // position when converted
    std::wstring name;
    std::wstring converted_name;   // name as converted; NPCs are renumbered from it each scan
    bool opened = false;           // TreasureChest: state when converted (fallback)
    std::wstring asset;            // NPC
    bool shop = false;             // NPC
    int dest_map_id = -1;          // MapExit
    std::wstring dest_name;        // MapExit
    std::wstring event_type;       // Event / ScriptTrigger: the raw English type ("ToLayer" ...)
    int transport_id = 0;          // Vehicle
    std::wstring message_id;       // Vehicle (FF4/FF5 official name)

    Category category() const;
    int priority() const;
    bool blocks_pathing() const;
    bool is_interactive() const;
    bool is_alive() const;
    unity::Vec3 position() const;
    bool is_opened() const;
    std::wstring display_name() const;
    std::wstring type_name() const;
    std::wstring format_description(unity::Vec3 player_pos) const;
};
using EntityPtr = std::shared_ptr<Entity>;

std::wstring direction_name(unity::Vec3 from, unity::Vec3 to);
std::wstring format_steps(float distance);
std::wstring localize_type_name(const std::wstring& type_name);
std::wstring vehicle_name(int id, const std::wstring& message_id);
float vec_distance(unity::Vec3 a, unity::Vec3 b);

// --- FilterContext (entity_scanner.cpp) -----------------------------------------------------
struct FilterContext {
    void* player_controller = nullptr;
    void* field_player = nullptr;
    void* map_handle = nullptr;
    unity::Vec3 player_position{};
    FilterContext();   // auto-populates from the current game state
};

// --- EntityScanner (entity_scanner.cpp) -----------------------------------------------------
namespace scanner {
void scan_entities();
void force_rescan();
void next_entity();
void previous_entity();
bool no_reachable_entities();
bool select_first_reachable();
void reapply_filter();
void set_filter_to_layer(bool on);
void set_category(Category c);
Category category();
EntityPtr current_entity();
int current_index();
int count();
std::vector<unity::Vec3> map_exit_positions();
} // namespace scanner

// --- player / map helpers (navigation.cpp) --------------------------------------------------
void* player_controller();          // GameObjectCache.Get<FieldPlayerController>() ?? Refresh
void* field_map();                  // GameObjectCache.Get<FieldMap>() ?? Refresh
// map_names.cpp: EntityScanner.GetCurrentMapId / MapNameResolver.GetCurrentMapIdInternal
int current_map_id_or_user_data();  // FieldMapProvisionInformation (> 0), then UserDataManager

// --- EntityNavigationManager (entity_nav.cpp) -----------------------------------------------
std::wstring category_name(Category c);

} // namespace field
