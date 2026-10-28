#pragma once

#include <QString>

#include <optional>
#include <vector>

namespace mirante {

struct FuzzyMatch
{
    int score = 0;                  // higher is better
    std::vector<int> positions;     // indices in the text of the matched characters
};

// Matches the characters of query in order, ignoring case, with bonuses for
// word starts and consecutive matches. Returns nullopt if not all characters
// are found. An empty query matches everything with score 0.
// Implemented in FuzzyMatch.cpp.
std::optional<FuzzyMatch> fuzzyMatch(const QString &query, const QString &text);

} // namespace mirante
