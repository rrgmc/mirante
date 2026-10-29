// Tests groupcolours: valid colours, the palette fallback by id, the colour of
// a task's group, and the colour offered to a new group.
#include "core/GroupColours.h"
#include "core/Model.h"

#include <QSet>
#include <QTest>

using namespace mirante;

namespace {

Group makeGroup(const QString &id, const QString &colour = QString())
{
    Group group;
    group.id = id;
    group.name = id;
    group.colour = colour;
    return group;
}

} // namespace

class TestGroupColours : public QObject
{
    Q_OBJECT

private slots:
    void paletteIsValidAndDistinct()
    {
        const QStringList &palette = groupcolours::palette();
        QVERIFY(palette.size() >= 6);
        QCOMPARE(QSet<QString>(palette.begin(), palette.end()).size(), palette.size());
        for (const QString &colour : palette) {
            QVERIFY2(groupcolours::isValid(colour), qPrintable(colour));
            QCOMPARE(colour, colour.toLower());
        }
    }

    void isValid_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("valid");
        QTest::newRow("lower") << "#3b82f6" << true;
        QTest::newRow("upper") << "#3B82F6" << true;
        QTest::newRow("empty") << "" << false;
        QTest::newRow("no hash") << "3b82f6" << false;
        QTest::newRow("short") << "#fff" << false;
        QTest::newRow("name") << "red" << false;
        QTest::newRow("not hex") << "#3b82fg" << false;
        QTest::newRow("alpha") << "#ff3b82f6" << false;
    }

    void isValid()
    {
        QFETCH(QString, text);
        QFETCH(bool, valid);
        QCOMPARE(groupcolours::isValid(text), valid);
    }

    // The fallback depends only on the id, so it is the same on every device
    // and every run. These values are fixed: changing the hash or the palette
    // changes the colour of every group without one.
    void fallbackIsStable()
    {
        const QString id = QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7");
        QCOMPARE(groupcolours::fallback(id), groupcolours::fallback(id));
        QCOMPARE(groupcolours::fallback(id), groupcolours::fallback(id.toUpper()));
        QVERIFY(groupcolours::palette().contains(groupcolours::fallback(id)));
        QCOMPARE(groupcolours::fallback(id), QStringLiteral("#14b8a6"));
        QCOMPARE(groupcolours::fallback(QStringLiteral("80314e00-c02f-5f8b-977f-0756b76406ea")),
                 QStringLiteral("#8b5cf6"));
    }

    void fallbackSpreadsOverThePalette()
    {
        QSet<QString> seen;
        for (int i = 0; i < 200; ++i)
            seen.insert(groupcolours::fallback(QStringLiteral("group-%1").arg(i)));
        QCOMPARE(seen.size(), groupcolours::palette().size());
    }

    void ofUsesOwnColourOrFallback()
    {
        QCOMPARE(groupcolours::of(makeGroup(QStringLiteral("g-1"), QStringLiteral("#AABBCC"))),
                 QStringLiteral("#aabbcc"));
        QCOMPARE(groupcolours::of(makeGroup(QStringLiteral("g-1"))), groupcolours::fallback(QStringLiteral("g-1")));
        // An invalid colour, perhaps from a newer version, falls back too.
        QCOMPARE(groupcolours::of(makeGroup(QStringLiteral("g-1"), QStringLiteral("red"))),
                 groupcolours::fallback(QStringLiteral("g-1")));
    }

    void ofTask()
    {
        Model model;
        Project project;
        project.id = QStringLiteral("p-1");
        project.groupId = QStringLiteral("g-1");
        Task task;
        task.id = QStringLiteral("t-1");
        task.projectId = project.id;
        model.reset({makeGroup(QStringLiteral("g-1"), QStringLiteral("#123456"))}, {project}, {task}, {});
        QCOMPARE(groupcolours::ofTask(model, QStringLiteral("t-1")), QStringLiteral("#123456"));
        QVERIFY(groupcolours::ofTask(model, QStringLiteral("unknown")).isEmpty());

        // The colour follows the group.
        model.setGroup(makeGroup(QStringLiteral("g-1"), QStringLiteral("#654321")));
        QCOMPARE(groupcolours::ofTask(model, QStringLiteral("t-1")), QStringLiteral("#654321"));
    }

    void nextTakesFirstUnused()
    {
        const QStringList &palette = groupcolours::palette();
        Model model;
        QCOMPARE(groupcolours::next(model), palette.at(0));

        model.reset({makeGroup(QStringLiteral("a"), palette.at(0)), makeGroup(QStringLiteral("b"), palette.at(2))},
                    {}, {}, {});
        QCOMPARE(groupcolours::next(model), palette.at(1));
    }

    void nextCountsFallbacks()
    {
        // A group without a colour uses its fallback, so a new group avoids it.
        Model model;
        const QString id = QStringLiteral("g-1");
        model.reset({makeGroup(id)}, {}, {}, {});
        QVERIFY(groupcolours::next(model) != groupcolours::fallback(id));
    }

    void nextWhenAllUsed()
    {
        const QStringList &palette = groupcolours::palette();
        std::vector<Group> groups;
        for (int i = 0; i < palette.size(); ++i)
            groups.push_back(makeGroup(QStringLiteral("g%1").arg(i), palette.at(i)));
        // The first colour is used twice, so the second is used least first.
        groups.push_back(makeGroup(QStringLiteral("extra"), palette.at(0)));
        Model model;
        model.reset(groups, {}, {}, {});
        QCOMPARE(groupcolours::next(model), palette.at(1));
    }
};

QTEST_GUILESS_MAIN(TestGroupColours)
#include "tst_group_colours.moc"
