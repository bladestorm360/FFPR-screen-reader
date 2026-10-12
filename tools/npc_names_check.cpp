// check-sources: field/npc_names.cpp
#include "field/npc_names.h"

#include <iostream>
#include <string>
#include <vector>

using field::npc_names::Entry;
using field::npc_names::Label;

namespace {

bool expect(std::wstring_view input, std::wstring_view base, long long ordinal)
{
    const Label got = field::npc_names::parse(input);
    if (got.base == base && got.ordinal == ordinal) return true;
    std::wcerr << L"parse mismatch for [" << input << L"]: base [" << got.base
               << L"], ordinal " << got.ordinal << L"\n";
    return false;
}

bool require(bool condition, const char* message)
{
    if (condition) return true;
    std::cerr << "check failed: " << message << "\n";
    return false;
}

Entry entry(std::wstring& output, std::wstring_view name, int map, int layer, float x, float y)
{
    return {&output, field::npc_names::parse(name), map, layer, x, y};
}

} // namespace

int main()
{
    // The requested eight-label sample: female/soldier duplicates and four unique labels.
    std::wstring female_a, female_b, soldier_a, soldier_b, old_woman, male, old_man, dancer;
    std::vector<Entry> eight{
        entry(female_a, L"8: Female Villager", 10, 0, 0, 0),
        entry(soldier_a, L"7: Soldier", 10, 0, 0, 0),
        entry(old_woman, L"5: Old Woman", 10, 0, 0, 0),
        entry(male, L"4: Male Villager", 10, 0, 0, 0),
        entry(old_man, L"6: Old Man", 10, 0, 0, 0),
        entry(dancer, L"3: Dancer", 10, 0, 0, 0),
        entry(soldier_b, L"2: Soldier", 10, 0, 0, 0),
        entry(female_b, L"1: Female Villager", 10, 0, 0, 0),
    };
    field::npc_names::renumber(eight);
    if (!require(female_a == L"Female Villager2" && female_b == L"Female Villager1", "eight-label female ordinals")) return 1;
    if (!require(soldier_a == L"Soldier2" && soldier_b == L"Soldier1", "eight-label soldier ordinals")) return 1;
    if (!require(old_woman == L"Old Woman" && male == L"Male Villager" && old_man == L"Old Man" && dancer == L"Dancer", "eight-label unique names")) return 1;

#define CHECK_LABEL(input, base, ordinal) if (!expect(input, base, ordinal)) return 1
    CHECK_LABEL(L"10: Female", L"Female", 10);
    CHECK_LABEL(L"２：女性", L"女性", 2);
    CHECK_LABEL(L"①女性", L"女性", 1);
    CHECK_LABEL(L"㉑女性", L"女性", 21);
    CHECK_LABEL(L"㊱女性", L"女性", 36);
    CHECK_LABEL(L"⒇女性", L"女性", 20);
    CHECK_LABEL(L"女性１０", L"女性", 10);
    CHECK_LABEL(L"女性⑳", L"女性", 20);
    CHECK_LABEL(L"女性⒇", L"女性", 20);
    CHECK_LABEL(L"SC12: Old woman", L"Old woman", 12);
    CHECK_LABEL(L"SC13. Old woman", L"Old woman", 13);
    CHECK_LABEL(L"NPC_14: Old woman", L"Old woman", 14);
    CHECK_LABEL(L"NPC15 Old woman", L"Old woman", 15);
    CHECK_LABEL(L"NPC Old woman", L"Old woman", -1);
    CHECK_LABEL(L"NPC", L"NPC", -1);
    CHECK_LABEL(L"NPC16", L"NPC", 16);
    CHECK_LABEL(L"NPC17:", L"NPC", 17);
    CHECK_LABEL(L"SC18:", L"SC18:", -1);
    CHECK_LABEL(L"sc_e_019_01:", L"sc_e_019_01:", -1);
    CHECK_LABEL(L"sc_e_017_02: 妖精", L"妖精", 17);
    CHECK_LABEL(L"sc_e_0036:NPC11フィガロ兵", L"フィガロ兵", 36);
    CHECK_LABEL(L"NPC1長老:ev_e_0430_1", L"長老", 1);
    CHECK_LABEL(L"ガラフ:ev_e_0121", L"ガラフ", 121);
    CHECK_LABEL(L"NPCセリス（オペラ衣装）:ev_e_0316（入れ替え用）", L"セリス（オペラ衣装）", 316);
    CHECK_LABEL(L"シド:ev_e_0615(ベットへ)", L"シド", 615);
    CHECK_LABEL(L"NPC帝国兵:sc_e_0111叩いた", L"帝国兵", 111);
    CHECK_LABEL(L"ケフカ:sc_e_0115~0118", L"ケフカ", 115);
    CHECK_LABEL(L"ev_e_018_03：妖精", L"妖精", 18);
    CHECK_LABEL(L"三闘神", L"三闘神", -1);
    CHECK_LABEL(L"123", L"123", -1);
    CHECK_LABEL(L"999999999999999999999999999999: Guard", L"Guard", 9223372036854775807LL);
    CHECK_LABEL(L"  ①:  女性  ", L"女性", 1);
    CHECK_LABEL(L"2: Woman10", L"Woman", 2); // a prefix ordinal wins over a suffix
    CHECK_LABEL(L"1: Soldier4", L"Soldier", 1);
    CHECK_LABEL(L"①村人②", L"村人", 1);
    CHECK_LABEL(L"NPC2_Merchant7", L"Merchant", 2);
    CHECK_LABEL(L"Soldier (guard)7", L"Soldier (guard)", 7);
    CHECK_LABEL(L"Soldier 1 7", L"Soldier", 1);
    CHECK_LABEL(L"Soldier (2)", L"Soldier", 2);
    CHECK_LABEL(L"Soldier.7", L"Soldier.", 7);
#undef CHECK_LABEL

    std::wstring ordered_a, ordered_b, ordered_c;
    std::vector<Entry> numeric_order{
        entry(ordered_a, L"Female10", 20, 0, 0, 0),
        entry(ordered_b, L"Female2", 20, 0, 0, 0),
        entry(ordered_c, L"Female", 20, 0, 0, 0),
    };
    field::npc_names::renumber(numeric_order);
    if (!require(ordered_b == L"Female1" && ordered_a == L"Female2" && ordered_c == L"Female3", "numeric 2 precedes 10")) return 1;
    field::npc_names::renumber(numeric_order);
    if (!require(ordered_b == L"Female1" && ordered_a == L"Female2" && ordered_c == L"Female3", "renumber is repeatable from original labels")) return 1;

    std::wstring floor_one_a, floor_one_b, floor_two, case_variant, other_tag;
    std::vector<Entry> independent{
        entry(floor_one_a, L"1: Guard", 30, 0, 0, 0),
        entry(floor_one_b, L"2: Guard", 30, 0, 1, 0),
        entry(floor_two, L"Guard", 31, 0, 0, 0),
        entry(case_variant, L"guard", 30, 0, 2, 0),
        entry(other_tag, L"1: Guard", 30, 1, -1, -1),
    };
    field::npc_names::renumber(independent);
    if (!require(floor_one_a == L"Guard1" && other_tag == L"Guard2" && floor_one_b == L"Guard3", "ordinal then layer ordering")) return 1;
    if (!require(floor_two == L"Guard" && case_variant == L"guard", "floor and case separation")) return 1;

    std::wstring shuffled_10, shuffled_2, shuffled_unknown_far, shuffled_unknown_near;
    std::vector<Entry> shuffled{
        entry(shuffled_unknown_far, L"Dancer", 40, 0, 9, 9),
        entry(shuffled_10, L"Dancer10", 40, 0, 0, 0),
        entry(shuffled_unknown_near, L"Dancer", 40, 0, 1, 1),
        entry(shuffled_2, L"Dancer2", 40, 0, 8, 8),
    };
    field::npc_names::renumber(shuffled);
    if (!require(shuffled_2 == L"Dancer1" && shuffled_10 == L"Dancer2", "shuffled known ordinal ordering")) return 1;
    if (!require(shuffled_unknown_near == L"Dancer3" && shuffled_unknown_far == L"Dancer4", "unknown ordering by coordinates")) return 1;
}
