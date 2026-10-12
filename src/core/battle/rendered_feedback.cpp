#include "rendered_feedback.h"
#include "battle.h"
#include "../displayed_ui.h"
#include "../frame.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"
#include "../ui/preferences.h"

namespace ffpr::battle::rendered {
void hit_count(void* renderer)
{
    if (!renderer || ui::prefs::damage_display() != 1) return;
    frame::every_frame([renderer] {
        if (!in_battle() || !unity::alive(renderer)) return false;
        if (!displayed::image_visible(displayed::field(renderer, "hitText"))) return true;
        void* array = displayed::field(renderer, "numRenderers");
        std::vector<void*> images;
        for (int i = 0; i < unity::array_length(array); ++i) images.push_back(unity::array_at(array, i));
        const auto count = displayed::sprite_number(images);
        if (!count) return true;
        ffpr::say(TF(*count == 1 ? L"{0} hit" : L"{0} hits", {text::num(*count)}), false);
        return false;
    }, "Battle.HitCount", 30);
}
void damage(void* renderer, bool miss, std::function<void(int)> speak)
{
    frame::every_frame([renderer, miss, speak] {
        if (!unity::alive(renderer)) return false;
        void* prefab = displayed::field(renderer, "prefab");
        void* transform = unity::transform(prefab);
        std::vector<void*> images;
        bool shown = false;
        // Native CreateDamageValue fills the Image on each immediate prefab
        // child, units-first. Descendant shadows/effects are not more digits.
        for (int i = 0; i < unity::child_count(transform); ++i) {
            void* image = unity::get_component(unity::child(transform, i), unity::image_class());
            images.push_back(image);
            shown = shown || displayed::image_visible(image);
        }
        if (!shown) return true;
        if (miss) { speak(0); return false; } // native ViewMiss and its visible image
        const auto digits = displayed::sprite_number(images);
        if (!digits) return true;
        speak(*digits);
        return false;
    }, "Battle.DamageDisplay", 60);
}
}
