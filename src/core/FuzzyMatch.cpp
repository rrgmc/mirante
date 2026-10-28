#include "core/FuzzyMatch.h"

#include <QChar>

#include <cstddef>
#include <limits>

// Scoring. Each matched character earns a base score, plus bonuses for where
// it is. Gaps between matched characters cost a little, and so does every
// character of the text, so that a shorter text wins when all else is equal.
//
// The matched positions are chosen by dynamic programming over (query
// character, text position), which finds the positions with the highest
// score. A greedy scan is cheaper but picks the first occurrence, so "mr" on
// "summary report" would take the "r" of "summary" instead of the word start
// of "report". The table is query length times text length, which is small for
// switcher entries. A greedy check runs first, so texts that do not match at
// all skip the table.
//
// The text is compared by code point, so letters outside the Basic
// Multilingual Plane fold their case too. Positions are UTF-16 indices into
// the text, as QString uses them.

namespace mirante {

namespace {

constexpr int kMatch = 16;          // every matched character
constexpr int kWordStart = 32;      // match at the start of a word
constexpr int kTextStart = 16;      // extra, for a match at the very start
constexpr int kConsecutive = 24;    // match right after the previous one
constexpr int kGapOpen = 6;         // first skipped character between matches
constexpr int kGapExtend = 2;       // each further skipped character
constexpr int kLength = 1;          // every code point of the text

constexpr int kNone = std::numeric_limits<int>::min() / 2;

struct CodePoint
{
    char32_t raw;
    char32_t folded;
    int index;      // UTF-16 index in the source string
};

std::vector<CodePoint> decode(const QString &s)
{
    std::vector<CodePoint> result;
    result.reserve(static_cast<std::size_t>(s.size()));
    for (qsizetype i = 0; i < s.size(); ++i) {
        const int index = static_cast<int>(i);
        char32_t cp = s.at(i).unicode();
        if (QChar::isHighSurrogate(cp) && i + 1 < s.size() && s.at(i + 1).isLowSurrogate()) {
            cp = QChar::surrogateToUcs4(s.at(i), s.at(i + 1));
            ++i;
        }
        result.push_back({cp, QChar::toCaseFolded(cp), index});
    }
    return result;
}

// A word starts at the start of the text, after a character that is neither a
// letter nor a digit (such as a space, "-", "_", "/" or "."), and where a
// lower case letter is followed by an upper case one, as in "camelCase".
bool isWordStart(const std::vector<CodePoint> &text, std::size_t j)
{
    if (j == 0)
        return true;
    const char32_t prev = text[j - 1].raw;
    const char32_t cur = text[j].raw;
    if (!QChar::isLetterOrNumber(prev) && QChar::isLetterOrNumber(cur))
        return true;
    return QChar::isLower(prev) && QChar::isUpper(cur);
}

bool isSubsequence(const std::vector<CodePoint> &query, const std::vector<CodePoint> &text)
{
    std::size_t i = 0;
    for (std::size_t j = 0; j < text.size() && i < query.size(); ++j) {
        if (query[i].folded == text[j].folded)
            ++i;
    }
    return i == query.size();
}

} // namespace

std::optional<FuzzyMatch> fuzzyMatch(const QString &query, const QString &text)
{
    if (query.isEmpty())
        return FuzzyMatch{};

    const std::vector<CodePoint> q = decode(query);
    const std::vector<CodePoint> t = decode(text);
    if (!isSubsequence(q, t))
        return std::nullopt;

    const std::size_t n = q.size();
    const std::size_t m = t.size();

    std::vector<int> bonus(m);
    for (std::size_t j = 0; j < m; ++j)
        bonus[j] = kMatch + (isWordStart(t, j) ? kWordStart : 0) + (j == 0 ? kTextStart : 0);

    // score[i * m + j]: the best score of matching query[0..i] with query[i]
    // at text position j, or kNone. from[i * m + j]: the position of
    // query[i - 1] in that match.
    std::vector<int> score(n * m, kNone);
    std::vector<std::size_t> from(n * m, 0);

    for (std::size_t j = 0; j < m; ++j) {
        if (q[0].folded == t[j].folded)
            score[j] = bonus[j];
    }

    for (std::size_t i = 1; i < n; ++i) {
        const int *prev = &score[(i - 1) * m];
        int *row = &score[i * m];
        std::size_t *rowFrom = &from[i * m];

        // The best previous match at least one character back, with its gap
        // penalty already applied for the current position.
        int gapBest = kNone;
        std::size_t gapFrom = 0;

        for (std::size_t j = i; j < m; ++j) {
            if (gapBest != kNone)
                gapBest -= kGapExtend;
            if (j >= 2 && prev[j - 2] != kNone && prev[j - 2] - kGapOpen >= gapBest) {
                gapBest = prev[j - 2] - kGapOpen;
                gapFrom = j - 2;
            }

            if (q[i].folded != t[j].folded)
                continue;

            int best = kNone;
            std::size_t bestFrom = 0;
            if (prev[j - 1] != kNone) {
                best = prev[j - 1] + kConsecutive;
                bestFrom = j - 1;
            }
            if (gapBest > best) {
                best = gapBest;
                bestFrom = gapFrom;
            }
            if (best != kNone) {
                row[j] = best + bonus[j];
                rowFrom[j] = bestFrom;
            }
        }
    }

    // The best end position; on a tie, the earliest.
    const int *last = &score[(n - 1) * m];
    std::size_t end = 0;
    int best = kNone;
    for (std::size_t j = 0; j < m; ++j) {
        if (last[j] > best) {
            best = last[j];
            end = j;
        }
    }

    FuzzyMatch match;
    match.score = best - kLength * static_cast<int>(m);
    match.positions.resize(n);
    std::size_t j = end;
    for (std::size_t i = n; i-- > 0;) {
        match.positions[i] = t[j].index;
        j = from[i * m + j];
    }
    return match;
}

} // namespace mirante
