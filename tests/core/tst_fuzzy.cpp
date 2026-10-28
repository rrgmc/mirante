// Tests fuzzy matching for the quick switcher: matches, positions and ranking.
#include "core/FuzzyMatch.h"

#include <QTest>

using namespace mirante;

namespace {

std::vector<int> positions(const QString &query, const QString &text)
{
    const std::optional<FuzzyMatch> match = fuzzyMatch(query, text);
    return match ? match->positions : std::vector<int>{-1};
}

// True if both texts match and the first scores higher.
bool ranksAbove(const QString &query, const QString &better, const QString &worse)
{
    const std::optional<FuzzyMatch> a = fuzzyMatch(query, better);
    const std::optional<FuzzyMatch> b = fuzzyMatch(query, worse);
    if (!a || !b) {
        qWarning() << "no match:" << query << (a ? worse : better);
        return false;
    }
    if (a->score <= b->score)
        qWarning() << query << better << a->score << "is not above" << worse << b->score;
    return a->score > b->score;
}

QString utf16(const char16_t *text)
{
    return QString::fromUtf16(text);
}

} // namespace

class TestFuzzy : public QObject
{
    Q_OBJECT

private slots:
    void emptyQuery()
    {
        for (const QString &text : {QString(), QString("anything")}) {
            const std::optional<FuzzyMatch> match = fuzzyMatch(QString(), text);
            QVERIFY(match);
            QCOMPARE(match->score, 0);
            QVERIFY(match->positions.empty());
        }
    }

    void noMatch()
    {
        QVERIFY(!fuzzyMatch("xyz", "backend"));
        QVERIFY(!fuzzyMatch("dkb", "backend"));             // out of order
        QVERIFY(!fuzzyMatch("backends", "backend"));        // longer than the text
        QVERIFY(!fuzzyMatch("a", QString()));
        QVERIFY(!fuzzyMatch("bb", "b"));                    // each text character matches once
    }

    void inOrderMatch()
    {
        QCOMPARE(positions("bkd", "backend"), std::vector<int>({0, 3, 6}));
        QCOMPARE(positions("backend", "backend"), std::vector<int>({0, 1, 2, 3, 4, 5, 6}));
    }

    void ignoresCase()
    {
        QCOMPARE(positions("MR", "mr review"), std::vector<int>({0, 1}));
        QCOMPARE(positions("mr", "MR REVIEW"), std::vector<int>({0, 1}));
        QCOMPARE(fuzzyMatch("MR", "mr review")->score, fuzzyMatch("mr", "mr review")->score);
    }

    void prefersWordStarts()
    {
        // The "r" of "report", not the earlier one in "summary".
        QCOMPARE(positions("mr", "summary report"), std::vector<int>({3, 8}));
        QCOMPARE(positions("gp", "gitlab-pipeline"), std::vector<int>({0, 7}));
        QCOMPARE(positions("dm", "data.model"), std::vector<int>({0, 5}));
        QCOMPARE(positions("us", "src/ui_store"), std::vector<int>({4, 7}));
        QCOMPARE(positions("fb", "fooBar"), std::vector<int>({0, 3}));
    }

    void prefersConsecutive()
    {
        QCOMPARE(positions("rev", "MR review"), std::vector<int>({3, 4, 5}));
        QCOMPARE(positions("view", "MR review"), std::vector<int>({5, 6, 7, 8}));
    }

    void ranking()
    {
        QVERIFY(ranksAbove("mr", "MR review", "summary report"));
        QVERIFY(ranksAbove("gp", "GitLab Pipeline", "group"));
        QVERIFY(ranksAbove("rep", "Report", "Prepare"));
        QVERIFY(ranksAbove("dash", "Dashboard", "Root dashboard"));
        QVERIFY(ranksAbove("ci", "CI status", "Archive"));
        QVERIFY(ranksAbove("pipe", "pipeline", "pick up the remaining work"));
    }

    void shorterTextWins()
    {
        QVERIFY(ranksAbove("api", "API", "API gateway"));
        QVERIFY(ranksAbove("task", "Task", "Tasks"));
    }

    void unicodeLetters()
    {
        // "ÇÃO" against "configuração": accented letters fold their case.
        QCOMPARE(positions(utf16(u"ÇÃO"), utf16(u"configuração")),
                 std::vector<int>({9, 10, 11}));
        // Accents are not stripped: "c" does not match "ç".
        QVERIFY(!fuzzyMatch("cao", utf16(u"ação")));
        // Greek "ΣΟΦ" against "σοφία".
        QCOMPARE(positions(utf16(u"ΣΟΦ"), utf16(u"σοφία")),
                 std::vector<int>({0, 1, 2}));
        // A letter outside the Basic Multilingual Plane takes two UTF-16 units.
        // Deseret capital U+10400 matches small U+10428, and positions are
        // UTF-16 indices.
        QCOMPARE(positions(utf16(u"\U00010400b"), utf16(u"a\U00010428b")), std::vector<int>({1, 3}));
        QCOMPARE(positions("B", utf16(u"\U00010428b")), std::vector<int>({2}));
    }
};

QTEST_GUILESS_MAIN(TestFuzzy)
#include "tst_fuzzy.moc"
