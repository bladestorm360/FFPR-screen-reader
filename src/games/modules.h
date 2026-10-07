#pragma once
#include "../core/game.h"

// The six game modules. Only dllmain.cpp includes this: the core never sees a game.
extern const game::Module kFF1Module;
extern const game::Module kFF2Module;
extern const game::Module kFF3Module;
extern const game::Module kFF4Module;
extern const game::Module kFF5Module;
extern const game::Module kFF6Module;

namespace games {
inline const game::Module* module_for(game::Id id)
{
    switch (id) {
    case game::Id::FF1: return &kFF1Module;
    case game::Id::FF2: return &kFF2Module;
    case game::Id::FF3: return &kFF3Module;
    case game::Id::FF4: return &kFF4Module;
    case game::Id::FF5: return &kFF5Module;
    case game::Id::FF6: return &kFF6Module;
    default: return nullptr;
    }
}
}
