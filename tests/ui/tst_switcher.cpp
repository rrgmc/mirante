// Tests the quick switcher: the candidate list, ranking, and the popup's keys.
#include "core/Model.h"
#include "core/Store.h"
#include "ui/QuickSwitcher.h"

#include <QLineEdit>
#include <QListWidget>
#include <QTest>

#include <algorithm>
#include <optional>

using namespace mirante;

namespace {

using Kind = SwitcherEntry::Kind;

const QString kFixLoginTask = QStringLiteral("760c8e20-8555-5a2c-bcbe-75e7fd08ca74");
const QString kAcme412Item = QStringLiteral("a2400d22-15bb-5bb0-be39-6a977ad25b0e");
const QString kWorkGroup = QStringLiteral("6d1b9bb9-f4c3-5677-a4e1-683ba618d4c7");

int kindOrder(Kind kind)
{
    switch (kind) {
    case Kind::Task:
        return 0;
    case Kind::Item:
        return 1;
    case Kind::Project:
        return 2;
    case Kind::RootDashboard:
        return 3;
    case Kind::Group:
        return 4;
    }
    return 5;
}

std::optional<SwitcherEntry> find(const std::vector<SwitcherEntry> &entries, Kind kind, const QString &id)
{
    for (const SwitcherEntry &entry : entries) {
        if (entry.kind == kind && entry.id == id)
            return entry;
    }
    return std::nullopt;
}

} // namespace

class TestSwitcher : public QObject
{
    Q_OBJECT

private:
    Model m_model;
    std::vector<SwitcherEntry> m_entries;

private slots:
    void initTestCase()
    {
        QVERIFY(store::load(QStringLiteral(MIRANTE_FIXTURES_DIR), m_model).empty());
        m_entries = QuickSwitcher::candidates(m_model);
    }

    void candidateCounts()
    {
        auto count = [this](Kind kind) {
            return static_cast<int>(std::count_if(m_entries.begin(), m_entries.end(),
                                                  [kind](const SwitcherEntry &entry) { return entry.kind == kind; }));
        };
        QCOMPARE(count(Kind::Group), 2);
        QCOMPARE(count(Kind::Project), 5);
        QCOMPARE(count(Kind::Task), 17);
        QCOMPARE(count(Kind::Item), 43);
        QCOMPARE(count(Kind::RootDashboard), 2);
        QCOMPARE(m_entries.size(), std::size_t(69));
    }

    void candidateDetails()
    {
        const std::optional<SwitcherEntry> group = find(m_entries, Kind::Group, kWorkGroup);
        QVERIFY(group);
        QCOMPARE(group->label, QStringLiteral("Work"));
        QVERIFY(group->detail.isEmpty());

        const std::optional<SwitcherEntry> task = find(m_entries, Kind::Task, kFixLoginTask);
        QVERIFY(task);
        QCOMPARE(task->label, QStringLiteral("Fix login timeout"));
        QCOMPARE(task->detail, QStringLiteral("Work / backend"));
        QVERIFY(task->taskId.isEmpty());

        const std::optional<SwitcherEntry> item = find(m_entries, Kind::Item, kAcme412Item);
        QVERIFY(item);
        QCOMPARE(item->label, QStringLiteral("ACME-412"));
        QCOMPARE(item->detail, QStringLiteral("backend / Fix login timeout"));
        QCOMPARE(item->taskId, kFixLoginTask);

        for (const SwitcherEntry &entry : m_entries) {
            if (entry.kind == Kind::RootDashboard)
                QCOMPARE(entry.detail, QStringLiteral("Dashboard"));
            if (entry.kind == Kind::Project && entry.label == QStringLiteral("Mirante"))
                QCOMPARE(entry.detail, QStringLiteral("Personal"));
        }
    }

    void emptyQueryOrder()
    {
        const std::vector<SwitcherEntry> ranked = QuickSwitcher::rank(m_entries, QString());
        QCOMPARE(ranked.size(), m_entries.size());
        for (std::size_t i = 1; i < ranked.size(); ++i) {
            const SwitcherEntry &a = ranked[i - 1];
            const SwitcherEntry &b = ranked[i];
            QVERIFY(kindOrder(a.kind) <= kindOrder(b.kind));
            if (a.kind == b.kind)
                QVERIFY(QString::compare(a.label, b.label, Qt::CaseInsensitive) <= 0);
        }
        QCOMPARE(ranked.front().label, QStringLiteral("Accessibility audit"));
        QCOMPARE(ranked.back().kind, Kind::Group);
        QCOMPARE(ranked.back().label, QStringLiteral("Work"));
    }

    void ranking()
    {
        // Equal scores: a task before an item.
        std::vector<SwitcherEntry> ranked = QuickSwitcher::rank(m_entries, QStringLiteral("login"));
        QVERIFY(ranked.size() >= 3);
        QCOMPARE(ranked[0].kind, Kind::Task);
        QCOMPARE(ranked[0].label, QStringLiteral("Fix login timeout"));
        QCOMPARE(ranked[1].kind, Kind::Item);
        QCOMPARE(ranked[1].label, QStringLiteral("Claude: login fix"));

        // A better score wins over the kind order.
        ranked = QuickSwitcher::rank(m_entries, QStringLiteral("work"));
        QVERIFY(!ranked.empty());
        QCOMPARE(ranked[0].kind, Kind::Group);
        QCOMPARE(ranked[0].label, QStringLiteral("Work"));

        ranked = QuickSwitcher::rank(m_entries, QStringLiteral("mirante"));
        QVERIFY(ranked.size() >= 2);
        QCOMPARE(ranked[0].kind, Kind::Project);
        QCOMPARE(ranked[1].label, QStringLiteral("Mirante folder"));

        // Word starts: "rl" picks "rate limiting".
        ranked = QuickSwitcher::rank(m_entries, QStringLiteral("rl"));
        QVERIFY(!ranked.empty());
        QCOMPARE(ranked[0].label, QStringLiteral("Add rate limiting"));

        // Exact matches come first, and identical labels keep the kind order:
        // the five Overview tasks before the item "Work overview".
        ranked = QuickSwitcher::rank(m_entries, QStringLiteral("overview"));
        QVERIFY(ranked.size() > 5);
        for (std::size_t i = 0; i < 5; ++i) {
            QCOMPARE(ranked[i].kind, Kind::Task);
            QCOMPARE(ranked[i].label, QStringLiteral("Overview"));
        }

        QVERIFY(QuickSwitcher::rank(m_entries, QStringLiteral("zzz")).empty());
    }

    void widgetKeys()
    {
        QWidget window;
        window.resize(800, 600);
        QuickSwitcher switcher(m_model, &window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(switcher.isHidden());

        auto *edit = switcher.findChild<QLineEdit *>();
        auto *list = switcher.findChild<QListWidget *>();
        QVERIFY(edit);
        QVERIFY(list);

        std::vector<SwitcherEntry> activated;
        connect(&switcher, &QuickSwitcher::activated, this,
                [&activated](const SwitcherEntry &entry) { activated.push_back(entry); });

        switcher.popup();
        QVERIFY(switcher.isVisible());
        QCOMPARE(list->count(), 69);
        QVERIFY(switcher.geometry().width() <= window.width());
        QVERIFY(switcher.geometry().center().x() >= window.width() / 2 - 1);
        QVERIFY(switcher.geometry().center().x() <= window.width() / 2 + 1);

        // "acme-" leaves ACME-398, ACME-412 and ACME-412 (page), in that order.
        QTest::keyClicks(edit, QStringLiteral("acme-"));
        QCOMPARE(list->count(), 3);
        QCOMPARE(list->currentRow(), 0);

        QTest::keyClick(edit, Qt::Key_Up);
        QCOMPARE(list->currentRow(), 0);
        QTest::keyClick(edit, Qt::Key_Down);
        QCOMPARE(list->currentRow(), 1);
        QTest::keyClick(edit, Qt::Key_Return);

        QCOMPARE(activated.size(), std::size_t(1));
        QCOMPARE(activated[0].kind, Kind::Item);
        QCOMPARE(activated[0].id, kAcme412Item);
        QCOMPARE(activated[0].taskId, kFixLoginTask);
        QVERIFY(switcher.isHidden());

        // Opening again clears the query.
        switcher.popup();
        QVERIFY(switcher.isVisible());
        QVERIFY(edit->text().isEmpty());
        QCOMPARE(list->count(), 69);

        // No match: Enter does nothing.
        QTest::keyClicks(edit, QStringLiteral("zzz"));
        QCOMPARE(list->count(), 0);
        QTest::keyClick(edit, Qt::Key_Return);
        QCOMPARE(activated.size(), std::size_t(1));
        QVERIFY(switcher.isVisible());

        QTest::keyClick(edit, Qt::Key_Escape);
        QVERIFY(switcher.isHidden());
        QCOMPARE(activated.size(), std::size_t(1));
    }

    void hidesOnFocusLoss()
    {
        QWidget window;
        window.resize(800, 600);
        auto *other = new QLineEdit(&window);
        QuickSwitcher switcher(m_model, &window);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));

        switcher.popup();
        QVERIFY(switcher.isVisible());
        QTRY_VERIFY(switcher.findChild<QLineEdit *>()->hasFocus());

        other->setFocus();
        QTRY_VERIFY(switcher.isHidden());
    }
};

QTEST_MAIN(TestSwitcher)
#include "tst_switcher.moc"
