#pragma once
#include <functional>

// Native data work only. Never access Unity objects, IL2CPP, or reader state here.
// Publish results through frame::post; this worker is never attached to the game GC.
namespace background {
enum class Lane { Data, Compute, Io, Navigation };
void prepare(); // boot only: park all lanes before native callbacks can publish work
bool post(std::function<void()> work, Lane lane = Lane::Data);
}
