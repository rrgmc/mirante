// The Folder / terminal view, with a fake context that records the programs
// it would start and answers Git reads by hand. No terminal or editor starts.
// See README.md#view.
#include "../FolderView.h"

#include <mirante/GitState.h>
#include <mirante/ItemContext.h>
#include <mirante/widgets/ActionRow.h>
#include <mirante/widgets/SummaryView.h>

#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTest>
#include <QTimer>

#include <functional>
#include <memory>

Q_DECLARE_METATYPE(mirante::GitState)

using mirante::GitState;
using mirante::widgets::ActionRow;
using mirante::widgets::SummaryView;

namespace folder = mirante::modules::folder;

namespace {

const QString kPath = QStringLiteral("C:/work/My Project");

class FakeContext : public mirante::ItemContext
{
public:
    QJsonObject itemConfig{{QStringLiteral("folder"), QStringLiteral("f1")}};
    QHash<QString, QString> paths{{QStringLiteral("f1"), kPath}};
    QHash<QString, QString> pathsAfterMap;
    QJsonObject deviceSettings;
    QString head;
    QString error;

    struct Start
    {
        QString dir;
        QString program;
        QStringList arguments;
    };
    QList<Start> terminals;
    QList<Start> programs;
    QStringList shownFolders;
    QStringList mapped;

    struct Read
    {
        QString path;
        QPointer<QObject> receiver;
        std::function<void(const GitState &)> done;
    };
    QList<Read> reads;

    QString itemName() const override { return QStringLiteral("Item"); }
    QJsonObject config() const override { return itemConfig; }
    QJsonValue projectSetting(const QString &) const override { return {}; }
    QJsonObject moduleSettings(const QString &) const override { return {}; }
    QString folderPath(const QString &folderId) const override { return paths.value(folderId); }
    void mapFolder(const QString &folderId) override
    {
        mapped << folderId;
        if (pathsAfterMap.contains(folderId))
            paths.insert(folderId, pathsAfterMap.value(folderId));
    }
    QWidget *createWebView(const QUrl &, QWidget *) override { return nullptr; }
    QWebEngineProfile *profile() override { return nullptr; }
    QWidget *createMessageView(const QString &, QWidget *) override { return nullptr; }
    void updateConfig(const QJsonObject &) override {}
    QJsonValue deviceSetting(const QString &key) const override
    {
        if (key == QLatin1String("editor") && !deviceSettings.contains(key))
            return QStringLiteral("code");
        return deviceSettings.value(key);
    }
    QString gitHead(const QString &) const override { return head; }
    void gitStatus(const QString &path, QObject *receiver, std::function<void(const GitState &)> done) override
    {
        reads << Read{path, receiver, std::move(done)};
    }
    QString launchTerminal(const QString &dir, const QString &program, const QStringList &arguments) override
    {
        terminals << Start{dir, program, arguments};
        return error;
    }
    QString startProgram(const QString &dir, const QString &program, const QStringList &arguments) override
    {
        programs << Start{dir, program, arguments};
        return error;
    }
    void showFolder(const QString &path) override { shownFolders << path; }

    // Answers the oldest read still waiting.
    void answer(const GitState &state)
    {
        QVERIFY(!reads.isEmpty());
        const Read read = reads.takeFirst();
        if (read.receiver)
            read.done(state);
    }
};

GitState okState()
{
    GitState state;
    state.result = GitState::Result::Ok;
    state.branch = QStringLiteral("main");
    state.commit = QStringLiteral("abc1234");
    state.hasUpstream = true;
    return state;
}

ActionRow *actionsOf(QWidget *view)
{
    return static_cast<SummaryView *>(view)->actions();
}

QPushButton *buttonOf(QWidget *view, const QString &id)
{
    return view->findChild<QPushButton *>(id);
}

QLabel *labelOf(QWidget *view, const QString &name)
{
    return view->findChild<QLabel *>(name);
}

// The text of a label, or empty while it is hidden.
QString shownText(QWidget *view, const QString &name)
{
    QLabel *label = labelOf(view, name);
    return label->isHidden() ? QString() : label->text();
}

// A tick of the refresh timer, without waiting for it. timeout() is a
// private signal, so it is called through the meta-object.
void fire(QTimer *timer)
{
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
}

} // namespace

class TestViews : public QObject
{
    Q_OBJECT

private slots:
    void showsNameAndPath()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));

        QVERIFY(labelOf(view.get(), QStringLiteral("path"))->text().contains(QStringLiteral("My Project")));
        const auto labels = view->findChildren<QLabel *>();
        bool titled = false;
        for (QLabel *label : labels)
            titled = titled || label->text() == QLatin1String("My Project");
        QVERIFY(titled);
        QVERIFY(actionsOf(view.get())->isActionVisible(QStringLiteral("terminal")));
        QVERIFY(!actionsOf(view.get())->isActionVisible(QStringLiteral("chooseFolder")));
    }

    void gitLines_data()
    {
        QTest::addColumn<GitState>("state");
        QTest::addColumn<QString>("branch");
        QTest::addColumn<QString>("status");

        const QString dot = QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");

        QTest::newRow("clean") << okState() << QStringLiteral("main") << QStringLiteral("Clean");

        GitState counts = okState();
        counts.ahead = 2;
        counts.changed = 3;
        counts.untracked = 1;
        QTest::newRow("counts") << counts << QStringLiteral("main")
                                << QStringLiteral("2 ahead") + dot + QStringLiteral("3 changed") + dot
                                       + QStringLiteral("1 untracked");

        GitState all = okState();
        all.ahead = 1;
        all.behind = 4;
        all.staged = 2;
        all.changed = 5;
        all.untracked = 6;
        all.conflicts = 1;
        QTest::newRow("all") << all << QStringLiteral("main")
                             << QStringLiteral("1 ahead") + dot + QStringLiteral("4 behind") + dot
                                    + QStringLiteral("2 staged") + dot + QStringLiteral("5 changed") + dot
                                    + QStringLiteral("6 untracked") + dot + QStringLiteral("1 conflict");

        GitState conflicts = okState();
        conflicts.conflicts = 2;
        QTest::newRow("conflicts") << conflicts << QStringLiteral("main") << QStringLiteral("2 conflicts");

        GitState detached = okState();
        detached.branch.clear();
        QTest::newRow("detached") << detached << QStringLiteral("Detached at abc1234") << QStringLiteral("Clean");

        GitState notRepository;
        notRepository.result = GitState::Result::NotARepository;
        QTest::newRow("not a repository") << notRepository << QString() << QStringLiteral("Not a Git repository");

        GitState missing;
        missing.result = GitState::Result::GitMissing;
        QTest::newRow("git missing") << missing << QStringLiteral("from-head") << QStringLiteral("Git was not found");

        GitState failed;
        failed.result = GitState::Result::Failed;
        failed.message = QStringLiteral("git status took too long.");
        QTest::newRow("failed") << failed << QStringLiteral("from-head")
                                << QStringLiteral("git status took too long.");
    }

    void gitLines()
    {
        QFETCH(GitState, state);
        QFETCH(QString, branch);
        QFETCH(QString, status);

        FakeContext context;
        context.head = QStringLiteral("from-head");
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        view->show();
        QCOMPARE(context.reads.size(), 1);
        QCOMPARE(context.reads.at(0).path, kPath);
        QCOMPARE(context.reads.at(0).receiver.data(), view.get());

        context.answer(state);
        QCOMPARE(shownText(view.get(), QStringLiteral("branch")), branch);
        QCOMPARE(shownText(view.get(), QStringLiteral("gitStatus")), status);
        // The actions work whatever Git says.
        QVERIFY(actionsOf(view.get())->isActionVisible(QStringLiteral("terminal")));
    }

    // Before Git answers, the branch comes from the head file.
    void branchFromHeadFirst()
    {
        FakeContext context;
        context.head = QStringLiteral("feature");
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        QCOMPARE(shownText(view.get(), QStringLiteral("branch")), QStringLiteral("feature"));
        QVERIFY(shownText(view.get(), QStringLiteral("gitStatus")).isEmpty());
    }

    void sameStateDrawsNothing()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        view->show();
        context.answer(okState());

        // A changed label text would show the view drew again.
        QLabel *status = labelOf(view.get(), QStringLiteral("gitStatus"));
        status->setText(QStringLiteral("marker"));
        QTimer *timer = view->findChild<QTimer *>(QStringLiteral("gitRefresh"));
        fire(timer);
        QCOMPARE(context.reads.size(), 1);
        context.answer(okState());
        QCOMPARE(status->text(), QStringLiteral("marker"));

        GitState changed = okState();
        changed.changed = 1;
        fire(timer);
        context.answer(changed);
        QCOMPARE(status->text(), QStringLiteral("1 changed"));
    }

    void oneReadAtATime()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        view->show();
        QTimer *timer = view->findChild<QTimer *>(QStringLiteral("gitRefresh"));
        fire(timer);
        QCOMPARE(context.reads.size(), 1);
    }

    void timerRunsOnlyWhileVisible()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        QTimer *timer = view->findChild<QTimer *>(QStringLiteral("gitRefresh"));
        QVERIFY(timer);
        QCOMPARE(timer->interval(), 10 * 1000);
        QVERIFY(!timer->isActive());
        QVERIFY(context.reads.isEmpty());

        view->show();
        QVERIFY(timer->isActive());
        QCOMPARE(context.reads.size(), 1);
        context.answer(okState());

        view->hide();
        QVERIFY(!timer->isActive());

        // Shown again, it reads at once.
        view->show();
        QVERIFY(timer->isActive());
        QCOMPARE(context.reads.size(), 1);
    }

    void terminalOpensAlone()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        buttonOf(view.get(), QStringLiteral("terminal"))->click();
        QCOMPARE(context.terminals.size(), 1);
        QCOMPARE(context.terminals.at(0).dir, kPath);
        QVERIFY(context.terminals.at(0).program.isEmpty());
        QVERIFY(context.terminals.at(0).arguments.isEmpty());
        QVERIFY(context.programs.isEmpty());
    }

    void editorCommand_data()
    {
        QTest::addColumn<QString>("setting");
        QTest::addColumn<QString>("program");
        QTest::addColumn<QStringList>("arguments");

        QTest::newRow("default") << QStringLiteral("code") << QStringLiteral("code") << QStringList{kPath};
        QTest::newRow("empty") << QStringLiteral("  ") << QStringLiteral("code") << QStringList{kPath};
        QTest::newRow("arguments") << QStringLiteral("code -n --reuse-window") << QStringLiteral("code")
                                   << QStringList{QStringLiteral("-n"), QStringLiteral("--reuse-window"), kPath};
        QTest::newRow("quoted program")
            << QStringLiteral("\"C:/Program Files/Editor/editor.exe\" --new")
            << QStringLiteral("C:/Program Files/Editor/editor.exe") << QStringList{QStringLiteral("--new"), kPath};
    }

    void editorCommand()
    {
        QFETCH(QString, setting);
        QFETCH(QString, program);
        QFETCH(QStringList, arguments);

        FakeContext context;
        context.deviceSettings.insert(QStringLiteral("editor"), setting);
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        buttonOf(view.get(), QStringLiteral("editor"))->click();
        QCOMPARE(context.programs.size(), 1);
        QCOMPARE(context.programs.at(0).dir, kPath);
        QCOMPARE(context.programs.at(0).program, program);
        QCOMPARE(context.programs.at(0).arguments, arguments);
    }

    void fileManagerShowsTheFolder()
    {
        FakeContext context;
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        buttonOf(view.get(), QStringLiteral("fileManager"))->click();
        QCOMPARE(context.shownFolders, QStringList{kPath});
    }

    void failureShowsItsMessage()
    {
        FakeContext context;
        context.error = QStringLiteral("The program could not be started.");
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));

        buttonOf(view.get(), QStringLiteral("editor"))->click();
        QCOMPARE(actionsOf(view.get())->error(), QStringLiteral("The program could not be started."));
        actionsOf(view.get())->clearError();
        buttonOf(view.get(), QStringLiteral("terminal"))->click();
        QCOMPARE(actionsOf(view.get())->error(), QStringLiteral("The program could not be started."));
    }

    void unmappedFolderChoosesOne()
    {
        FakeContext context;
        context.paths.clear();
        context.pathsAfterMap.insert(QStringLiteral("f1"), QStringLiteral("D:/mapped"));
        std::unique_ptr<QWidget> view(folder::createFolderView(context, nullptr));
        view->show();

        QCOMPARE(labelOf(view.get(), QStringLiteral("path"))->text(), QStringLiteral("Folder not set on this device"));
        QVERIFY(context.reads.isEmpty());
        QVERIFY(!actionsOf(view.get())->isActionVisible(QStringLiteral("terminal")));
        QVERIFY(!actionsOf(view.get())->isActionVisible(QStringLiteral("editor")));
        QVERIFY(!actionsOf(view.get())->isActionVisible(QStringLiteral("fileManager")));
        QVERIFY(actionsOf(view.get())->isActionVisible(QStringLiteral("chooseFolder")));

        // The view reads the path again after the picker, and Git with it.
        buttonOf(view.get(), QStringLiteral("chooseFolder"))->click();
        QCOMPARE(context.mapped, QStringList{QStringLiteral("f1")});
        QVERIFY(labelOf(view.get(), QStringLiteral("path"))->text().contains(QStringLiteral("mapped")));
        QVERIFY(actionsOf(view.get())->isActionVisible(QStringLiteral("terminal")));
        QVERIFY(!actionsOf(view.get())->isActionVisible(QStringLiteral("chooseFolder")));
        QCOMPARE(context.reads.size(), 1);
        QCOMPARE(context.reads.at(0).path, QStringLiteral("D:/mapped"));
    }
};

QTEST_MAIN(TestViews)
#include "tst_views.moc"
