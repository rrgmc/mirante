// Tests the web profiles of core/WebProfiles.h: the resolution order of item,
// group and Default, the fallback for a missing profile, the list offered to
// the user, names, and removing a profile.
#include "core/Entities.h"
#include "core/Model.h"
#include "core/WebProfiles.h"

#include <QSignalSpy>
#include <QTest>

using namespace mirante;

namespace {

WebProfile makeProfile(const QString &id, const QString &name)
{
    WebProfile profile;
    profile.id = id;
    profile.name = name;
    return profile;
}

} // namespace

class TestWebProfiles : public QObject
{
    Q_OBJECT

private:
    Model model;

    // Group g1 holds project p1, which holds task t1 with item i1. The
    // workspace lists "work" and "bank".
    void load(const QString &groupProfile, const QString &itemProfile)
    {
        Group group;
        group.id = QStringLiteral("g1");
        group.webProfile = groupProfile;
        Project project;
        project.id = QStringLiteral("p1");
        project.groupId = group.id;
        Item item;
        item.id = QStringLiteral("i1");
        item.type = QStringLiteral("web.page");
        item.webProfile = itemProfile;
        Task task;
        task.id = QStringLiteral("t1");
        task.projectId = project.id;
        task.items.push_back(item);
        model.reset({group}, {project}, {task}, {});
        model.setWebProfiles({makeProfile(QStringLiteral("work"), QStringLiteral("Work")),
                              makeProfile(QStringLiteral("bank"), QStringLiteral("bank"))});
    }

    QString effective() const { return webprofiles::effective(model, QStringLiteral("t1"), QStringLiteral("i1")); }

private slots:
    void resolutionOrder_data()
    {
        QTest::addColumn<QString>("groupProfile");
        QTest::addColumn<QString>("itemProfile");
        QTest::addColumn<QString>("expected");

        QTest::newRow("nothing set") << QString() << QString() << kDefaultWebProfile;
        QTest::newRow("group default") << QStringLiteral("work") << QString() << QStringLiteral("work");
        QTest::newRow("item override") << QStringLiteral("work") << QStringLiteral("bank") << QStringLiteral("bank");
        QTest::newRow("item override, no group default") << QString() << QStringLiteral("bank") << QStringLiteral("bank");
        QTest::newRow("item chooses Default") << QStringLiteral("work") << QStringLiteral("default") << kDefaultWebProfile;
        QTest::newRow("group names Default") << QStringLiteral("default") << QString() << kDefaultWebProfile;
        // A missing profile, deleted or not synced yet, is skipped.
        QTest::newRow("missing group profile") << QStringLiteral("gone") << QString() << kDefaultWebProfile;
        QTest::newRow("missing item profile") << QStringLiteral("work") << QStringLiteral("gone") << QStringLiteral("work");
        QTest::newRow("both missing") << QStringLiteral("gone") << QStringLiteral("gone") << kDefaultWebProfile;
    }

    void resolutionOrder()
    {
        QFETCH(QString, groupProfile);
        QFETCH(QString, itemProfile);
        QFETCH(QString, expected);
        load(groupProfile, itemProfile);
        QCOMPARE(effective(), expected);
    }

    void unknownItemGivesDefault()
    {
        load(QStringLiteral("work"), QString());
        QCOMPARE(webprofiles::effective(model, QStringLiteral("t1"), QStringLiteral("nope")), kDefaultWebProfile);
        Item loose;
        loose.webProfile = QStringLiteral("gone");
        QCOMPARE(webprofiles::effective(model, loose, nullptr), kDefaultWebProfile);
    }

    void offeredDefaultFirstThenByName()
    {
        load(QString(), QString());
        const std::vector<WebProfile> all = webprofiles::all(model);
        QCOMPARE(all.size(), std::size_t(3));
        QCOMPARE(all[0].id, kDefaultWebProfile);
        QCOMPARE(all[0].name, QStringLiteral("Default"));
        QCOMPARE(all[1].id, QStringLiteral("bank"));   // names compare ignoring case
        QCOMPARE(all[2].id, QStringLiteral("work"));

        QVERIFY(webprofiles::exists(model, kDefaultWebProfile));
        QVERIFY(webprofiles::exists(model, QStringLiteral("work")));
        QVERIFY(!webprofiles::exists(model, QStringLiteral("gone")));
        QVERIFY(!webprofiles::exists(model, QString()));
        QCOMPARE(webprofiles::nameOf(model, kDefaultWebProfile), QStringLiteral("Default"));
        QCOMPARE(webprofiles::nameOf(model, QStringLiteral("work")), QStringLiteral("Work"));
        QVERIFY(webprofiles::nameOf(model, QStringLiteral("gone")).isEmpty());
    }

    // Removing a profile leaves the group and item that name it as they are.
    // They fall back: the item to its group's profile, the group to Default.
    void removeFallsBack()
    {
        load(QStringLiteral("work"), QStringLiteral("bank"));
        QSignalSpy changed(&model, &Model::webProfilesChanged);

        QVERIFY(webprofiles::remove(model, QStringLiteral("bank")));
        QCOMPARE(changed.count(), 1);
        QVERIFY(!model.webProfile(QStringLiteral("bank")));
        QCOMPARE(model.item(QStringLiteral("t1"), QStringLiteral("i1"))->webProfile, QStringLiteral("bank"));
        QCOMPARE(effective(), QStringLiteral("work"));

        QVERIFY(webprofiles::remove(model, QStringLiteral("work")));
        QCOMPARE(model.group(QStringLiteral("g1"))->webProfile, QStringLiteral("work"));
        QCOMPARE(effective(), kDefaultWebProfile);
        QVERIFY(model.webProfiles().empty());

        // Default and unknown ids cannot be removed.
        QVERIFY(!webprofiles::remove(model, kDefaultWebProfile));
        QVERIFY(!webprofiles::remove(model, QStringLiteral("work")));
        QCOMPARE(changed.count(), 2);

        // The profile comes back, for example when its entry is synced again,
        // and its users use it again.
        model.setWebProfiles({makeProfile(QStringLiteral("work"), QStringLiteral("Work"))});
        QCOMPARE(effective(), QStringLiteral("work"));
    }

    void resetClearsTheList()
    {
        load(QString(), QString());
        model.reset({}, {}, {}, {});
        QVERIFY(model.webProfiles().empty());
    }
};

QTEST_GUILESS_MAIN(TestWebProfiles)
#include "tst_web_profiles.moc"
