#pragma once
#include "../field/baked_nav.h"

namespace ffpr::fieldnav::autowalk {
bool active();
/// All input devices use this start/stop contract. A second request stops the walk.
bool stop_requested();
void request(field::baked_nav::MapGoal goal, field::TargetKind kind, std::wstring name);
void start(const field::PathInfo& path, const field::baked_nav::MapGoal* goal = nullptr,
           field::TargetKind kind = field::TargetKind::Spot, const std::wstring& name = L"");
void cancel(bool announce = false);
/// Called after keyboard/SDL polling, before dispatch, even with no keyboard key pressed.
void input_updated();
/// Native input checks see a one-frame confirm/back press for a planned vehicle action.
bool virtual_key(int action, bool up = false);
bool virtual_input_active();
}
