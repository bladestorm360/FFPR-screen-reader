#pragma once
#include <functional>

// Native data work only. Never access Unity objects, IL2CPP, or reader state here.
// Publish results through frame::post; this worker is never attached to the game GC.
namespace background {
bool post(std::function<void()> work);
}
