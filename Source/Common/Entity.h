//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once

#include "Common.h"

#include "Properties.h"
#include "TextPack.h"

///@ ExportEntity Game FOServer FOClient Global
///@ ExportEntity Player Player PlayerView
///@ ExportEntity Item Item ItemView HasProto HasStatics HasAbstract
///@ ExportEntity Critter Critter CritterView HasProto
///@ ExportEntity Map Map MapView HasProto
///@ ExportEntity Location Location LocationView HasProto

#define ENTITY_PROPERTY(access_type, prop_type, prop) \
    inline auto GetProperty##prop() const->const Property* \
    { \
        return _propsRef.GetRegistrator()->GetByIndexFast(prop##_RegIndex); \
    } \
    inline prop_type Get##prop() const \
    { \
        return _propsRef.GetValue<prop_type>(GetProperty##prop()); \
    } \
    inline void Set##prop(prop_type value) \
    { \
        _propsRef.SetValue(GetProperty##prop(), value); \
    } \
    inline bool IsNonEmpty##prop() const \
    { \
        return _propsRef.GetRawDataSize(GetProperty##prop()) > 0u; \
    } \
    static uint16 prop##_RegIndex

#define ENTITY_EVENT(event_name, ...) \
    EntityEvent<__VA_ARGS__> event_name \
    { \
        this, #event_name \
    }

///@ ExportEnum
enum class CritterItemSlot : uint8
{
    Inventory = 0,
    Main = 1,
    Outside = 255,
};

///@ ExportEnum
enum class CritterCondition : uint8
{
    Alive = 0,
    Knockout = 1,
    Dead = 2,
};

// Critter actions
// Flags for chosen:
// l - hardcoded local call
// s - hardcoded server call
// for all others critters actions call only server
//  flags actionExt item
///@ ExportEnum
enum class CritterAction : uint16
{
    None = 0,
    MoveItem = 2,
    SwapItems = 3,
    DropItem = 5,
    Knockout = 16,
    StandUp = 17,
    Fidget = 18,
    Dead = 19,
    Connect = 20,
    Disconnect = 21,
    Respawn = 22,
    Refresh = 23,
};

///@ ExportEnum
enum class CritterStateAnim : uint16
{
    None = 0,
    Unarmed = 1,
};

///@ ExportEnum
enum class CritterActionAnim : uint16
{
    None = 0,
    Idle = 1,
    Walk = 3,
    WalkBack = 15,
    Limp = 4,
    Run = 5,
    RunBack = 16,
    TurnRight = 17,
    TurnLeft = 18,
    PanicRun = 6,
    SneakWalk = 7,
    SneakRun = 8,
    IdleProneFront = 86,
    DeadFront = 102,
};

///@ ExportEnum
enum class CritterFindType : uint8
{
    Any = 0,
    Alive = 0x01,
    Dead = 0x02,
    Players = 0x10,
    Npc = 0x20,
    AlivePlayers = 0x11,
    DeadPlayers = 0x12,
    AliveNpc = 0x21,
    DeadNpc = 0x22,
};

///@ ExportEnum
enum class ItemOwnership : uint8
{
    MapHex = 0,
    CritterInventory = 1,
    ItemContainer = 2,
    Nowhere = 3,
};

///@ ExportEnum
enum class ContainerItemStack : uint
{
    Root = 0,
    Any = 0xFFFFFFFF,
};

///@ ExportEnum
enum class CornerType : uint8
{
    NorthSouth = 0,
    West = 1,
    East = 2,
    South = 3,
    North = 4,
    EastWest = 5,
};

class AnimationResolver
{
public:
    virtual ~AnimationResolver() = default;
    [[nodiscard]] virtual auto ResolveCritterAnimation(hstring model_name, CritterStateAnim state_anim, CritterActionAnim action_anim, uint& pass, uint& flags, int& ox, int& oy, string& anim_name) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationSubstitute(hstring base_model_name, CritterStateAnim base_state_anim, CritterActionAnim base_action_anim, hstring& model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim) -> bool = 0;
    [[nodiscard]] virtual auto ResolveCritterAnimationFallout(hstring model_name, CritterStateAnim& state_anim, CritterActionAnim& action_anim, CritterStateAnim& state_anim_ex, CritterActionAnim& action_anim_ex, uint& flags) -> bool = 0;
};

class EntityProperties
{
protected:
    explicit EntityProperties(Properties& props);

    Properties& _propsRef;
};

class Entity
{
    friend class EntityEventBase;

public:
    using EventCallback = std::function<bool(const initializer_list<void*>&)>;

    ///@ ExportEnum
    enum class EventExceptionPolicy
    {
        IgnoreAndContinueChain,
        StopChainAndReturnTrue,
        StopChainAndReturnFalse,
        PropogateException,
    };

    ///@ ExportEnum
    enum class EventPriority
    {
        Lowest,
        Low,
        Normal,
        High,
        Highest,
    };

    struct EventCallbackData
    {
        EventCallback Callback {};
        const void* SubscribtionPtr {};
        EventExceptionPolicy ExPolicy {EventExceptionPolicy::IgnoreAndContinueChain}; // Todo: improve entity event ExPolicy
        EventPriority Priority {EventPriority::Normal}; // Todo: improve entity event Priority
        bool OneShot {}; // Todo: improve entity event OneShot
        bool Deferred {}; // Todo: improve entity event Deferred
    };

    Entity() = delete;
    Entity(const Entity&) = delete;
    Entity(Entity&&) noexcept = delete;
    auto operator=(const Entity&) = delete;
    auto operator=(Entity&&) noexcept = delete;

    [[nodiscard]] virtual auto GetName() const -> string_view = 0;
    [[nodiscard]] virtual auto IsGlobal() const -> bool { return false; }
    [[nodiscard]] auto GetClassName() const -> string_view;
    [[nodiscard]] auto GetProperties() const -> const Properties&;
    [[nodiscard]] auto GetPropertiesForEdit() -> Properties&;
    [[nodiscard]] auto IsDestroying() const -> bool;
    [[nodiscard]] auto IsDestroyed() const -> bool;
    [[nodiscard]] auto GetValueAsInt(const Property* prop) const -> int;
    [[nodiscard]] auto GetValueAsInt(int prop_index) const -> int;
    [[nodiscard]] auto GetValueAsAny(const Property* prop) const -> any_t;
    [[nodiscard]] auto GetValueAsAny(int prop_index) const -> any_t;

    void StoreData(bool with_protected, vector<const uint8*>** all_data, vector<uint>** all_data_sizes) const;
    void RestoreData(const vector<vector<uint8>>& props_data);
    void SetValueFromData(const Property* prop, PropertyRawData& prop_data);
    void SetValueAsInt(const Property* prop, int value);
    void SetValueAsInt(int prop_index, int value);
    void SetValueAsAny(const Property* prop, const any_t& value);
    void SetValueAsAny(int prop_index, const any_t& value);
    void SubscribeEvent(const string& event_name, EventCallbackData callback);
    void UnsubscribeEvent(const string& event_name, const void* subscription_ptr);
    void UnsubscribeAllEvent(const string& event_name);
    auto FireEvent(const string& event_name, const initializer_list<void*>& args) -> bool;

    void AddRef() const noexcept;
    void Release() const noexcept;

    void MarkAsDestroying();
    virtual void MarkAsDestroyed();

protected:
    Entity(const PropertyRegistrator* registrator, const Properties* props);
    virtual ~Entity() = default;

    auto GetInitRef() -> Properties& { return _props; }

    bool _nonConstHelper {};

private:
    auto GetEventCallbacks(const string& event_name) -> vector<EventCallbackData>*;
    void SubscribeEvent(vector<EventCallbackData>* callbacks, EventCallbackData callback);
    void UnsubscribeEvent(vector<EventCallbackData>* callbacks, const void* subscription_ptr);
    auto FireEvent(vector<EventCallbackData>* callbacks, const initializer_list<void*>& args) -> bool;

    Properties _props;
    unordered_map<string, vector<EventCallbackData>> _events {};
    bool _isDestroying {};
    bool _isDestroyed {};
    mutable int _refCounter {1};
};

class ProtoEntity : public Entity
{
public:
    [[nodiscard]] auto GetName() const -> string_view override;
    [[nodiscard]] auto GetProtoId() const -> hstring;
    [[nodiscard]] auto HasComponent(hstring name) const -> bool;
    [[nodiscard]] auto HasComponent(hstring::hash_t hash) const -> bool;
    [[nodiscard]] auto GetComponents() const -> unordered_set<hstring> { return _components; }

    void EnableComponent(hstring component);

    vector<pair<string, TextPack>> Texts {};
    string CollectionName {};

protected:
    ProtoEntity(hstring proto_id, const PropertyRegistrator* registrator, const Properties* props);

    const hstring _protoId;
    unordered_set<hstring> _components {};
    unordered_set<hstring::hash_t> _componentHashes {};
};

class EntityWithProto
{
public:
    EntityWithProto() = delete;
    EntityWithProto(const EntityWithProto&) = delete;
    EntityWithProto(EntityWithProto&&) noexcept = delete;
    auto operator=(const EntityWithProto&) = delete;
    auto operator=(EntityWithProto&&) noexcept = delete;

    [[nodiscard]] auto GetProtoId() const -> hstring;
    [[nodiscard]] auto GetProto() const -> const ProtoEntity*;

protected:
    explicit EntityWithProto(const ProtoEntity* proto);
    virtual ~EntityWithProto();

    const ProtoEntity* _proto;
};

class EntityEventBase
{
public:
    void Subscribe(Entity::EventCallbackData callback);
    void Unsubscribe(const void* subscription_ptr);
    void UnsubscribeAll();

protected:
    EntityEventBase(Entity* entity, const char* callback_name);

    [[nodiscard]] auto FireEx(const initializer_list<void*>& args_list) const -> bool { return _entity->FireEvent(_callbacks, args_list); }

    Entity* _entity;
    const char* _callbackName;
    vector<Entity::EventCallbackData>* _callbacks {};
};

template<typename... Args>
class EntityEvent final : public EntityEventBase
{
public:
    EntityEvent(Entity* entity, const char* callback_name) :
        EntityEventBase(entity, callback_name)
    {
    }

    auto Fire(Args... args) -> bool
    {
        if (_callbacks == nullptr) {
            return true;
        }

        const initializer_list<void*> args_list = {&args...};
        return FireEx(args_list);
    }
};
