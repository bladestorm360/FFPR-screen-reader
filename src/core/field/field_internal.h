#pragma once
// Internal declarations shared by the files of the field area (src/core/field). Not for other
// areas: they use field_api.h, map_names.h, entity_translator.h, object_cache.h and
// scene_events.h. (NavigationTargetTracker and LocationMessageTracker live in src/core/fieldnav.)
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_set>
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
extern ref::Field fColliderEntityList;
extern ref::Field fTransportation;
extern ref::Field fFootEvent;
extern ref::Field fMapManager;
extern ref::Method mIsCanMoveToDestPosition;

extern ref::Class kIMapAccessor;
extern ref::Method mGetCollisionLayerWidth;
extern ref::Method mGetCollisionLayerHeight;

extern ref::Class kTransportationController;
extern ref::Field fInfoData;
extern ref::Method mCheckNeedInteractive;
extern ref::Method mGetCurrentTransportation;
extern ref::Class kTransportation;
extern ref::Field fModelList;
extern ref::Class kTransportationInfo;
extern ref::Method mInfoGetEnable;
extern ref::Method mInfoGetType;
extern ref::Method mInfoGetId;
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
extern ref::Field fPropEntityId;
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

extern ref::Class kMapManager;
extern ref::Field fCurrentMapModel;
extern ref::Class kMapModel;

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
/// object found in Transportation.ModelList, its master id and its message id (official name).
bool vehicle_type_of(void* field_entity, int& type, int& transportation_id, std::wstring& message_id);

/// The vehicles parked on this map (Transportation.ModelList objects seen at the last scan),
/// with their positions now: the baked router's boarding candidates (baked_nav.cpp).
struct VehicleOnMap {
    void* game = nullptr;
    int type = 0;
    int transportation_id = 0;
    std::wstring message_id;
    unity::Vec3 pos{};
};
std::vector<VehicleOnMap> vehicles_on_map();
/// The transportation groups of the party's enabled auto-ridden vehicles (type 5 "Content":
/// FF1-FF3's canoe, FF2's snowcraft), read from Transportation.ModelList now. On a world map
/// they extend walking: the player boards them by stepping onto their terrain.
std::vector<int> owned_auto_vehicle_groups();

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
    std::wstring audio_hint;       // bounded raw identifying string; never spoken
    bool audio_door = false;       // positive native/name door evidence, not an exit guess
    bool audio_exit = false;       // actual GotoMap detector, including unknown destination
    bool opened = false;           // TreasureChest: state when converted (fallback)
    bool opened_known = true;      // Remote chest: false if save flag/mapping is unavailable
    std::wstring asset;            // NPC
    bool shop = false;             // NPC
    int dest_map_id = -1;          // MapExit
    std::wstring dest_name;        // MapExit
    std::wstring event_type;       // Event / ScriptTrigger: the raw English type ("ToLayer" ...)
    int transport_id = 0;          // Vehicle: MapConstants.TransportationType
    int transportation_id = 0;     // Vehicle: transportation master id (0 = unknown)
    std::wstring message_id;       // Vehicle: the master's message_id (its official name)
    // An entry from another floor of this map, listed from the baked data under the "Current
    // map" filter (user, 2026-10-11): no FieldEntity, no live position. Routed by its cell.
    int remote_map_id = 0;         // that floor's map id; 0 = a live entity on this floor
    int cell_layer = 1, cell_x = 0, cell_y = 0;
    std::wstring floor_name;       // the floor's title, spoken after the name

    bool remote() const { return remote_map_id > 0; }
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
void when_filter_ready(std::function<void()> ready, bool existing_selection = false);
void scan_entities();
void schedule_scan();
void cancel_scan();
void invalidate_map();
bool needs_scan(int map_id);
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
double captured_at(); // oldest read in the currently published row snapshot, QPC seconds
std::vector<unity::Vec3> map_exit_positions();
} // namespace scanner

// --- player / map helpers (navigation.cpp) --------------------------------------------------
void* player_controller();          // GameObjectCache.Get<FieldPlayerController>() ?? Refresh
void* field_map();                  // GameObjectCache.Get<FieldMap>() ?? Refresh
// Native-owner cursor: one current collection slot per call. Never transfer it or
// returned engine objects to a worker; callers capture ready values immediately.
struct EntityCursor {
    int stage = 0, index = 0;
    std::unordered_set<void*> seen;
    std::vector<int> auto_groups;
    bool next(void*& entity);
};
bool native_position_walkable(void* player, unity::Vec3 position);
// map_names.cpp: EntityScanner.GetCurrentMapId / MapNameResolver.GetCurrentMapIdInternal
int current_map_id_or_user_data();  // FieldMapProvisionInformation (> 0), then UserDataManager

// --- EntityNavigationManager (entity_nav.cpp) -----------------------------------------------
std::wstring category_name(Category c);

} // namespace field
