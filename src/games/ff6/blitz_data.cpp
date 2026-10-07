// Ports FF6 Utils/BlitzData.cs (Sabin's Blitz input sequences, keyed by the ability's MesIdName).
//
// The tokens are mod text keys (data/common/mod_text.json), localized when the line is built,
// as the C# T(token) did.
#include <string>
#include <vector>
#include "ability.h"
#include "../../core/modtext.h"
#include "../../core/text.h"

namespace ffpr::ff6 {
namespace {

constexpr int kMaxTokens = 9;

struct BlitzSequence {
    const wchar_t* mes_id_name;
    int count;
    const wchar_t* tokens[kMaxTokens];
};

// BlitzData.BlitzSequences
constexpr BlitzSequence kSequences[] = {
    // Raging Fist: Left, Right, Left
    {L"MSG_MAGIC_NAME_109", 3, {L"Left", L"Right", L"Left"}},
    // Aura Cannon: Down, Down-Left, Left
    {L"MSG_MAGIC_NAME_110", 3, {L"Down", L"Down-Left", L"Left"}},
    // Meteor Strike: R2, L2, Down, Up
    {L"MSG_MAGIC_NAME_111", 4, {L"R2", L"L2", L"Down", L"Up"}},
    // Rising Phoenix: Left, Down-Left, Down, Down-Right, Right
    {L"MSG_MAGIC_NAME_112", 5, {L"Left", L"Down-Left", L"Down", L"Down-Right", L"Right"}},
    // Chakra: R2, L2, R2, L2, Down, Up
    {L"MSG_MAGIC_NAME_113", 6, {L"R2", L"L2", L"R2", L"L2", L"Down", L"Up"}},
    // Razor Gale: Up, Up-Right, Right, Down-Right, Down, Down-Left, Left
    {L"MSG_MAGIC_NAME_114", 7, {L"Up", L"Up-Right", L"Right", L"Down-Right", L"Down", L"Down-Left", L"Left"}},
    // Soul Spiral: R2, L2, Up, Down, Right, Left
    {L"MSG_MAGIC_NAME_115", 6, {L"R2", L"L2", L"Up", L"Down", L"Right", L"Left"}},
    // Phantom Rush: Left, Down-Left, Up, Up-Right, Right, Down-Right, Down, Down-Left, Left
    {L"MSG_MAGIC_NAME_116", 9,
     {L"Left", L"Down-Left", L"Up", L"Up-Right", L"Right", L"Down-Right", L"Down", L"Down-Left", L"Left"}},
};

} // namespace

bool try_get_blitz_sequence(const std::wstring& mes_id_name, std::wstring& sequence)
{
    sequence.clear();
    if (mes_id_name.empty()) return false;

    for (const BlitzSequence& b : kSequences) {
        if (mes_id_name != b.mes_id_name) continue;
        // "Left, Right, Left, then Confirm"
        std::vector<std::wstring> parts;
        parts.reserve(static_cast<size_t>(b.count) + 1);
        for (int i = 0; i < b.count; ++i) parts.push_back(T(b.tokens[i]));
        parts.push_back(T(L"then Confirm"));
        sequence = TF(L"Blitz input: {0}", {text::join(parts, L", ")});
        return true;
    }
    return false;
}

} // namespace ffpr::ff6
