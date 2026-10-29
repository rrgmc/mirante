// Tests the profiles of core/Profiles.h: the resolution order of item, group
// and Default, the fallback for a missing profile, the list offered to the
// user, names, removing a profile, and module settings by profile in the
// model.
#include "core/Entities.h"
#include "core/Model.h"
#include "core/Profiles.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace mirante;

namespace {

Profile makeProfile(const QString &id, const QString &name)
{
    Profile profile;
    profile.id = id;
    profile.name = name;
    return profile;
}

} // namespace

class TestProfiles : public QObject
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
        group.profile = groupProfile;
        Project project;
        project.id = QStringLiteral("p1");
        project.groupId = group.id;
        Item item;
        item.id = QStringLiteral("i1");
        item.type = QStringLiteral("web.page");
        item.profile = itemProfile;
        Task task;
        task.id = QStringLiteral("t1");
        task.projectId = project.id;
        task.items.push_back(item);
        model.reset({group}, {project}, {task}, {});
        model.setProfiles({makeProfile(QStringLiteral("work"), QStringLiteral("Work")),
                           makeProfile(QStringLiteral("bank"), QStringLiteral("bank"))});
    }

    QString effective() const { return profiles::effective(model, QStringLiteral("t1"), QStringLiteral("i1")); }

private slots:
    void resolutionOrder_data()
    {
        QTest::addColumn<QString>("groupProfile");
        QTest::addColumn<QString>("itemProfile");
        QTest::addColumn<QString>("expected");

        QTest::newRow("nothing set") << QString() << QString() << kDefaultProfile;
        QTest::newRow("group default") << QStringLiteral("work") << QString() << QStringLiteral("work");
        QTest::newRow("item override") << QStringLiteral("work") << QStringLiteral("bank") << QStringLiteral("bank");
        QTest::newRow("item override, no group default") << QString() << QStringLiteral("bank") << QStringLiteral("bank");
        QTest::newRow("item chooses Default") << QStringLiteral("work") << QStringLiteral("default") << kDefaultProfile;
        QTest::newRow("group names Default") << QStringLiteral("default") << QString() << kDefaultProfile;
        // A missing profile, deleted or not synced yet, is skipped.
        QTest::newRow("missing group profile") << QStringLiteral("gone") << QString() << kDefaultProfile;
        QTest::newRow("missing item profile") << QStringLiteral("work") << QStringLiteral("gone") << QStringLiteral("work");
        QTest::newRow("both missing") << QStringLiteral("gone") << QStringLiteral("gone") << kDefaultProfile;
    }

    void resolutionOrder()
    {
        QFETCH(QString, groupProfile);
        QFETCH(QString, itemProfile);
        QFETCH(QString, expected);
        load(groupProfile, itemProfile);
        QCOMPARE(effective(), expected);
        // The same order for an item being edited, before it is saved.
        QCOMPARE(profiles::effectiveFor(model, QStringLiteral("t1"), itemProfile), expected);
    }

    void unknownItemGivesDefault()
    {
        load(QStringLiteral("work"), QString());
        QCOMPARE(profiles::effective(model, QStringLiteral("t1"), QStringLiteral("nope")), kDefaultProfile);
        Item loose;
        loose.profile = QStringLiteral("gone");
        QCOMPARE(profiles::effective(model, loose, nullptr), kDefaultProfile);
    }

    void offeredDefaultFirstThenByName()
    {
        load(QString(), QString());
        const std::vector<Profile> all = profiles::all(model);
        QCOMPARE(all.size(), std::size_t(3));
        QCOMPARE(all[0].id, kDefaultProfile);
        QCOMPARE(all[0].name, QStringLiteral("Default"));
        QCOMPARE(all[1].id, QStringLiteral("bank"));   // names compare ignoring case
        QCOMPARE(all[2].id, QStringLiteral("work"));

        QVERIFY(profiles::exists(model, kDefaultProfile));
        QVERIFY(profiles::exists(model, QStringLiteral("work")));
        QVERIFY(!profiles::exists(model, QStringLiteral("gone")));
        QVERIFY(!profiles::exists(model, QString()));
        QCOMPARE(profiles::nameOf(model, kDefaultProfile), QStringLiteral("Default"));
        QCOMPARE(profiles::nameOf(model, QStringLiteral("work")), QStringLiteral("Work"));
        QVERIFY(profiles::nameOf(model, QStringLiteral("gone")).isEmpty());
    }

    // Removing a profile leaves the group and item that name it as they are.
    // They fall back: the item to its group's profile, the group to Default.
    void removeFallsBack()
    {
        load(QStringLiteral("work"), QStringLiteral("bank"));
        QSignalSpy changed(&model, &Model::profilesChanged);

        QVERIFY(profiles::remove(model, QStringLiteral("bank")));
        QCOMPARE(changed.count(), 1);
        QVERIFY(!model.profile(QStringLiteral("bank")));
        QCOMPARE(model.item(QStringLiteral("t1"), QStringLiteral("i1"))->profile, QStringLiteral("bank"));
        QCOMPARE(effective(), QStringLiteral("work"));

        QVERIFY(profiles::remove(model, QStringLiteral("work")));
        QCOMPARE(model.group(QStringLiteral("g1"))->profile, QStringLiteral("work"));
        QCOMPARE(effective(), kDefaultProfile);
        // Only Default is left.
        QCOMPARE(model.profiles().size(), std::size_t(1));
        QCOMPARE(model.profiles().front().id, kDefaultProfile);

        // Default and unknown ids cannot be removed.
        QVERIFY(!profiles::remove(model, kDefaultProfile));
        QVERIFY(!profiles::remove(model, QStringLiteral("work")));
        QCOMPARE(changed.count(), 2);

        // The profile comes back, for example when its entry is synced again,
        // and its users use it again.
        model.setProfiles({makeProfile(QStringLiteral("work"), QStringLiteral("Work"))});
        QCOMPARE(effective(), QStringLiteral("work"));
    }

    void resetClearsTheList()
    {
        load(QString(), QString());
        model.reset({}, {}, {}, {});
        QCOMPARE(model.profiles().size(), std::size_t(1));
        QCOMPARE(model.profiles().front().id, kDefaultProfile);
    }

    // Default is always first, once, and keeps no name. setProfile() adds or
    // replaces one profile.
    void defaultAlwaysFirst()
    {
        Profile standard = makeProfile(kDefaultProfile, QStringLiteral("Mine"));
        standard.moduleSettings = QJsonObject{{QStringLiteral("gitlab"), QJsonObject{}}};
        model.setProfiles({makeProfile(QStringLiteral("work"), QStringLiteral("Work")), standard});
        QCOMPARE(model.profiles().size(), std::size_t(2));
        QCOMPARE(model.profiles()[0].id, kDefaultProfile);
        QVERIFY(model.profiles()[0].name.isEmpty());
        QVERIFY(model.profiles()[0].hasData());
        QCOMPARE(model.profiles()[1].id, QStringLiteral("work"));

        QSignalSpy changed(&model, &Model::profilesChanged);
        model.setProfile(makeProfile(QStringLiteral("bank"), QStringLiteral("Bank")));
        QCOMPARE(model.profiles().size(), std::size_t(3));
        model.setProfile(makeProfile(QStringLiteral("bank"), QStringLiteral("Bank 2")));
        QCOMPARE(model.profile(QStringLiteral("bank"))->name, QStringLiteral("Bank 2"));
        // A value equal to the stored one emits nothing.
        model.setProfile(makeProfile(QStringLiteral("bank"), QStringLiteral("Bank 2")));
        QCOMPARE(changed.count(), 2);
    }

    // Module settings live in each profile. Clearing a screen's values
    // removes its entry, then its module's.
    void moduleSettingsByProfile()
    {
        load(QString(), QString());
        const QString gitlab = QStringLiteral("gitlab");
        const QString general = QStringLiteral("general");
        const QJsonObject paths{{QStringLiteral("projects"), QJsonArray{QStringLiteral("acme/web")}}};
        const QJsonObject other{{QStringLiteral("x"), 1}};
        QSignalSpy changed(&model, &Model::profilesChanged);

        model.setModuleSettings(QStringLiteral("work"), gitlab, general, paths);
        model.setModuleSettings(QStringLiteral("work"), gitlab, QStringLiteral("other"), other);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(model.moduleSettings(QStringLiteral("work"), gitlab, general), paths);
        QCOMPARE(model.moduleSettings(QStringLiteral("work"), gitlab, QStringLiteral("other")), other);
        QVERIFY(model.moduleSettings(kDefaultProfile, gitlab, general).isEmpty());
        QVERIFY(model.moduleSettings(QStringLiteral("bank"), gitlab, general).isEmpty());
        QVERIFY(model.moduleSettings(QStringLiteral("gone"), gitlab, general).isEmpty());

        model.setModuleSettings(kDefaultProfile, gitlab, general, paths);
        QCOMPARE(model.moduleSettings(kDefaultProfile, gitlab, general), paths);

        // An unknown profile is not created.
        model.setModuleSettings(QStringLiteral("gone"), gitlab, general, paths);
        QVERIFY(!model.profile(QStringLiteral("gone")));

        model.setModuleSettings(QStringLiteral("work"), gitlab, general, {});
        QVERIFY(model.moduleSettings(QStringLiteral("work"), gitlab, general).isEmpty());
        QCOMPARE(model.moduleSettings(QStringLiteral("work"), gitlab, QStringLiteral("other")), other);
        model.setModuleSettings(QStringLiteral("work"), gitlab, QStringLiteral("other"), {});
        QVERIFY(model.profile(QStringLiteral("work"))->moduleSettings.isEmpty());
        QCOMPARE(model.moduleSettings(kDefaultProfile, gitlab, general), paths);
    }
};

QTEST_GUILESS_MAIN(TestProfiles)
#include "tst_profiles.moc"
