#include "field_api.h"
#include "../refs.h"
#include <cmath>

namespace field {
namespace {
ref::Class kFacingPlayer{"Last.Entity.Field", "FieldPlayer"};
ref::Class kFacingEntity{"Last.Entity.Field", "FieldEntity"};
ref::Class kFacingSprite{"Last.Entity.Field", "FieldSpriteEntity"};
ref::Method mFacingLookAt{kFacingSprite, "LookAt", 2};
ref::Method mFacingIsMoving{kFacingEntity, "get_IsMoving", 0};
ref::Field fFacingVector{kFacingEntity, "directionVector"};
}

bool face_player(void* player, int direction)
{
    if (direction < 0 || direction > 3 || !unity::alive(player) || !kFacingPlayer.is(player)
        || field::player() != player || !mFacingLookAt.ptr() || !mFacingIsMoving.ptr()
        || fFacingVector.get() < 0) return false;
    try {
        using Moving = bool (*)(void*, const void*);
        if (reinterpret_cast<Moving>(mFacingIsMoving.ptr())(player, mFacingIsMoving.get())) return false;
        static constexpr unity::Vec2 headings[] = {{0,1}, {1,0}, {0,-1}, {-1,0}};
        const auto heading = headings[direction];
        // FieldSpriteEntity.LookAt is the player's inherited override. It updates the
        // direction vector and native idle sprite; it never asks the player to move.
        using Look = void (*)(void*, unity::Vec2, bool, const void*);
        reinterpret_cast<Look>(mFacingLookAt.ptr())(player, heading, true, mFacingLookAt.get());
        const auto actual = ref::rd<unity::Vec2>(player, fFacingVector);
        return std::fabs(actual.x - heading.x) < .01f && std::fabs(actual.y - heading.y) < .01f;
    } catch (...) { ref::call_failed_note("Field.FacePlayer"); return false; }
}
}
