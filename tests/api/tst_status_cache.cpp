// Tests StatusCache with a test item type whose fetches end as each test
// wants, and a clock the test moves: what is due under each rule, the poll
// interval, fetching at once, one job per item, errors keeping data, stuck
// jobs, changes and removals in the model, the item's source, folder paths,
// and Git heads watched for a job.
#include "StatusTestSupport.h"

#include "core/GitHead.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>

#include <algorithm>

using namespace mirante;
using namespace mirante::statustest;

class TestStatusCache : public QObject
{
    Q_OBJECT

    std::unique_ptr<Fixture> f;

    // Fetched since the last call.
    QStringList takeFetched()
    {
        const QStringList fetched = f->controller.fetched;
        f->controller.fetched.clear();
        return fetched;
    }

    // Gives project p1 the main folder f1 and the folder f2.
    void addFolders()
    {
        Project project = *f->model.project(QStringLiteral("p1"));
        Folder f1;
        f1.id = QStringLiteral("f1");
        f1.main = true;
        Folder f2;
        f2.id = QStringLiteral("f2");
        project.folders = {f1, f2};
        f->model.setProject(project);
    }

    // Makes a Git working tree in dir with its head on branch.
    static bool makeRepository(const QString &dir, const QString &branch)
    {
        return QDir(dir).mkpath(QStringLiteral(".git")) && writeHead(dir, branch);
    }

    // Writes the head as Git does: a new file in place of the old one.
    static bool writeHead(const QString &dir, const QString &branch)
    {
        const QString head = dir + QStringLiteral("/.git/HEAD");
        const QString lock = head + QStringLiteral(".lock");
        QFile file(lock);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        file.write("ref: refs/heads/" + branch.toUtf8() + '\n');
        file.close();
        QFile::remove(head);
        return QFile::rename(lock, head);
    }

    // The item's job that runs now. Earlier held jobs of the item were
    // dropped by the cache, and may be deleted.
    StatusJob *runningJob(const QString &itemId) const
    {
        StatusJob *last = nullptr;
        for (const auto &[id, job] : f->controller.held) {
            if (id == itemId)
                last = job;
        }
        return last;
    }

    // Ends the item's running job, reading the head of dir first as a type
    // that follows a branch does, and forgets its held jobs.
    void finishHeld(const QString &itemId, const QString &dir)
    {
        StatusJob *job = runningJob(itemId);
        QVERIFY(job);
        f->controller.held.erase(std::remove_if(f->controller.held.begin(), f->controller.held.end(),
                                                [&](const auto &held) { return held.first == itemId; }),
                                 f->controller.held.end());
        if (!dir.isEmpty())
            job->gitHead(dir);
        job->finish(okStatus(itemId));
    }

    // Waits until the item was fetched again, and a moment more for repeated
    // change signals, then forgets what was fetched.
    bool waitForFetch(const QString &itemId)
    {
        QElapsedTimer timer;
        timer.start();
        while (!f->controller.fetched.contains(itemId) && timer.elapsed() < 5000)
            QTest::qWait(20);
        QTest::qWait(200);
        const bool fetched = f->controller.fetched.contains(itemId);
        f->controller.fetched.clear();
        return fetched;
    }

    // Writes the head and waits for the item to be fetched again. On Windows
    // the watcher starts watching a new file in the background, so a write
    // made at once after the watch was added can go unseen on a slow machine,
    // as on CI. It waits a moment first, and writes again up to twice.
    bool switchHead(const QString &dir, const QString &branch, const QString &itemId)
    {
        for (int attempt = 0; attempt < 3; ++attempt) {
            QTest::qWait(300);
            if (!writeHead(dir, branch))
                return false;
            if (waitForFetch(itemId))
                return true;
        }
        return false;
    }

private slots:
    void init() { f = std::make_unique<Fixture>(); }
    void cleanup() { f.reset(); }

    // Without visible cells or opened tasks, only the items of active Urgent
    // and High tasks are due: not a done one, not one in the trash.
    void urgentAndHighActiveTasksAreDue()
    {
        f->cache->poll();
        QStringList fetched = takeFetched();
        fetched.sort();
        QCOMPARE(fetched, QStringList({QStringLiteral("i4"), QStringLiteral("i7")}));
    }

    // A watched item of an active task is due at any priority. One of an
    // archived task, of a task in the trash, or of a type without status is
    // not. See decision 0087.
    void watchedItemsAreDue()
    {
        for (const auto &[taskId, itemId] : {std::pair{"t1", "i1"}, std::pair{"t1", "i2"},
                                             std::pair{"t4", "i6"}, std::pair{"t6", "i8"}}) {
            Item item = *f->model.item(QLatin1String(taskId), QLatin1String(itemId));
            item.watch = true;
            f->model.setItem(QLatin1String(taskId), item);
        }
        f->cache->poll();
        QStringList fetched = takeFetched();
        fetched.sort();
        QCOMPARE(fetched, QStringList({QStringLiteral("i1"), QStringLiteral("i4"), QStringLiteral("i7")}));
    }

    void visibleItemsAreDue()
    {
        f->cache->poll();
        takeFetched();
        // Shown with no status: fetched at once.
        f->cache->setVisible(QStringLiteral("i1"), true);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i1")}));
        // A second cell of the same item.
        f->cache->setVisible(QStringLiteral("i1"), true);
        QVERIFY(takeFetched().isEmpty());
        f->cache->setVisible(QStringLiteral("i1"), false);

        f->advance(120);
        f->cache->poll();
        QVERIFY(takeFetched().contains(QStringLiteral("i1")));

        // Both cells hidden: no longer due.
        f->cache->setVisible(QStringLiteral("i1"), false);
        f->advance(120);
        f->cache->poll();
        QVERIFY(!takeFetched().contains(QStringLiteral("i1")));
    }

    // A visible item of a done task is due too.
    void visibleDoneTaskIsDue()
    {
        f->cache->setVisible(QStringLiteral("i5"), true);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i5")}));
    }

    void openedTasksAreDueFor30Minutes()
    {
        f->cache->noteTaskOpened(QStringLiteral("t1"));
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i1")}));

        f->advance(29 * 60);
        f->cache->poll();
        QVERIFY(takeFetched().contains(QStringLiteral("i1")));

        f->advance(2 * 60);
        f->cache->poll();
        QVERIFY(!takeFetched().contains(QStringLiteral("i1")));

        // Opened again: due again.
        f->cache->noteTaskOpened(QStringLiteral("t1"));
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i1")}));
    }

    // Items in the trash, items of a missing module, and items of a type
    // without status are never fetched, not even when visible or refreshed.
    void neverDue()
    {
        for (const QString &itemId : {QStringLiteral("i2"), QStringLiteral("i3"), QStringLiteral("i6")}) {
            f->cache->setVisible(itemId, true);
            f->cache->refresh(itemId);
        }
        f->cache->noteTaskOpened(QStringLiteral("t1"));
        f->cache->poll();
        QVERIFY(!f->controller.fetched.contains(QStringLiteral("i2")));
        QVERIFY(!f->controller.fetched.contains(QStringLiteral("i3")));
        QVERIFY(!f->controller.fetched.contains(QStringLiteral("i6")));
        QCOMPARE(f->cache->status(QStringLiteral("i6")).state, ItemStatus::State::None);
    }

    void intervalComesFromSettings()
    {
        f->cache->poll();
        QVERIFY(takeFetched().contains(QStringLiteral("i4")));
        f->advance(119);
        f->cache->poll();
        QVERIFY(takeFetched().isEmpty());
        f->advance(1);
        f->cache->poll();
        QVERIFY(takeFetched().contains(QStringLiteral("i4")));

        f->device->setPollInterval(300);
        f->advance(299);
        f->cache->poll();
        QVERIFY(takeFetched().isEmpty());
        f->advance(1);
        f->cache->poll();
        QVERIFY(takeFetched().contains(QStringLiteral("i4")));
    }

    void refreshFetchesAtOnce()
    {
        f->cache->poll();
        takeFetched();
        f->cache->refresh(QStringLiteral("i4"));
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i4")}));
        // Through the source too, and for an item that is not due.
        f->cache->source(QStringLiteral("i1")).refresh();
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i1")}));
    }

    void oneJobPerItem()
    {
        f->controller.mode = Fetch::Hold;
        f->cache->poll();
        f->cache->refresh(QStringLiteral("i4"));
        f->cache->setVisible(QStringLiteral("i4"), true);
        // Still due and without a status, but the job is not stuck yet.
        f->advance(60);
        f->cache->poll();
        QCOMPARE(f->controller.fetched.count(QStringLiteral("i4")), 1);

        // Once it ends, the next fetch runs.
        for (const auto &[itemId, job] : f->controller.held)
            job->finish(okStatus(itemId));
        f->controller.held.clear();
        QCOMPARE(f->cache->status(QStringLiteral("i4")).summary, QStringLiteral("i4"));
        f->cache->refresh(QStringLiteral("i4"));
        QCOMPARE(f->controller.fetched.count(QStringLiteral("i4")), 2);
    }

    void okStatusIsKept()
    {
        QSignalSpy changed(f->cache.get(), &StatusCache::statusChanged);
        f->controller.next[QStringLiteral("i4")] = okStatus(QStringLiteral("Open"));
        f->cache->poll();
        const ItemStatus status = f->cache->status(QStringLiteral("i4"));
        QCOMPARE(status.state, ItemStatus::State::Ok);
        QCOMPARE(status.summary, QStringLiteral("Open"));
        // The cache stamps the time the data came.
        QCOMPARE(status.updatedAt, f->now);
        QVERIFY(changed.contains(QVariantList{QStringLiteral("i4")}));
        QCOMPARE(f->cache->source(QStringLiteral("i4")).current().summary, QStringLiteral("Open"));
    }

    void errorKeepsData()
    {
        f->controller.next[QStringLiteral("i4")] =
            okStatus(QStringLiteral("Open"), {reason(QStringLiteral("pipeline-failed"))});
        f->cache->poll();
        const QDateTime fetched = f->now;

        ItemStatus error;
        error.state = ItemStatus::State::Error;
        error.message = QStringLiteral("Network down");
        f->controller.next[QStringLiteral("i4")] = error;
        f->advance(120);
        f->cache->poll();

        const ItemStatus status = f->cache->status(QStringLiteral("i4"));
        QCOMPARE(status.state, ItemStatus::State::Error);
        QCOMPARE(status.message, QStringLiteral("Network down"));
        QCOMPARE(status.summary, QStringLiteral("Open"));
        QCOMPARE(status.data.value(QStringLiteral("summary")).toString(), QStringLiteral("Open"));
        QCOMPARE(status.attention.size(), 1);
        QCOMPARE(status.updatedAt, fetched);

        // An error without earlier data is kept as it is.
        ItemStatus noToken;
        noToken.state = ItemStatus::State::NoToken;
        noToken.message = QStringLiteral("Token not set");
        f->controller.next[QStringLiteral("i1")] = noToken;
        f->cache->refresh(QStringLiteral("i1"));
        QCOMPARE(f->cache->status(QStringLiteral("i1")).state, ItemStatus::State::NoToken);
        QVERIFY(!f->cache->status(QStringLiteral("i1")).updatedAt.isValid());

        // An error is not fetched again before the interval.
        takeFetched();
        f->advance(60);
        f->cache->poll();
        QVERIFY(!takeFetched().contains(QStringLiteral("i4")));
    }

    void stuckJobTimesOut()
    {
        f->controller.mode = Fetch::Hold;
        f->cache->poll();
        QVERIFY(!f->controller.held.isEmpty());
        f->advance(89);
        f->cache->poll();
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::None);

        f->advance(1);
        f->cache->poll();
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::Error);
        QVERIFY(!f->cache->status(QStringLiteral("i4")).message.isEmpty());

        // A late answer of the dropped job is ignored. The job is deleted on
        // the next pass of the event loop, so it may still be called now.
        for (const auto &[itemId, job] : f->controller.held) {
            if (itemId == QStringLiteral("i4"))
                job->finish(okStatus(QStringLiteral("late")));
        }
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::Error);

        // The job and its owner go.
        QPointer<QObject> owner = f->controller.held.first().second->owner();
        f->controller.held.clear();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(owner.isNull());
    }

    // Without a server for the module, a request through the job's server
    // ends with NoServer, on a later pass of the event loop.
    void requestWithoutServer()
    {
        f->controller.mode = Fetch::Server;
        f->cache->refresh(QStringLiteral("i1"));
        QCOMPARE(f->cache->status(QStringLiteral("i1")).state, ItemStatus::State::None);
        QTRY_COMPARE(f->cache->status(QStringLiteral("i1")).state, ItemStatus::State::NoServer);
        QVERIFY(!f->cache->status(QStringLiteral("i1")).message.isEmpty());
    }

    void configChangeFetchesAtOnce()
    {
        f->cache->poll();
        takeFetched();
        Task task = *f->model.task(QStringLiteral("t2"));
        task.items[0].config.insert(QStringLiteral("iid"), 7);
        f->model.setTask(task);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i4")}));

        // A change that leaves the item alone fetches nothing.
        task.title = QStringLiteral("Renamed");
        f->model.setTask(task);
        QVERIFY(takeFetched().isEmpty());

        // A change to the item's profile.
        Profile work;
        work.id = QStringLiteral("work");
        work.name = QStringLiteral("Work");
        std::vector<Profile> profiles = f->model.profiles();
        profiles.push_back(work);
        f->model.setProfiles(profiles);
        QVERIFY(takeFetched().isEmpty());
        task.items[0].profile = work.id;
        f->model.setTask(task);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i4")}));

        // A change to the module's settings in that profile.
        f->model.setModuleSettings(work.id, QStringLiteral("test"), QStringLiteral("general"),
                                   QJsonObject{{QStringLiteral("baseUrl"), QStringLiteral("https://example.com")}});
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i4")}));

        // A change to the project's settings of the module.
        Project project = *f->model.project(QStringLiteral("p1"));
        project.moduleSettings.insert(QStringLiteral("test"), QJsonObject{{QStringLiteral("project"), QStringLiteral("a/b")}});
        f->model.setProject(project);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i4")}));
    }

    // A job running when the configuration changes is dropped: its answer
    // is for the old configuration.
    void configChangeDropsRunningJob()
    {
        f->controller.mode = Fetch::Hold;
        f->cache->poll();
        Task task = *f->model.task(QStringLiteral("t2"));
        task.items[0].config.insert(QStringLiteral("iid"), 7);
        f->model.setTask(task);
        QCOMPARE(f->controller.fetched.count(QStringLiteral("i4")), 2);

        StatusJob *oldJob = nullptr;
        StatusJob *newJob = nullptr;
        for (const auto &[itemId, job] : f->controller.held) {
            if (itemId == QStringLiteral("i4"))
                (oldJob ? newJob : oldJob) = job;
        }
        QVERIFY(oldJob && newJob);
        QCOMPARE(newJob->config().value(QStringLiteral("iid")).toInt(), 7);
        oldJob->finish(okStatus(QStringLiteral("old")));
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::None);
        newJob->finish(okStatus(QStringLiteral("new")));
        QCOMPARE(f->cache->status(QStringLiteral("i4")).summary, QStringLiteral("new"));
    }

    void jobReadsSettings()
    {
        f->model.setModuleSettings(kDefaultProfile, QStringLiteral("test"), QStringLiteral("general"),
                                   QJsonObject{{QStringLiteral("baseUrl"), QStringLiteral("https://example.com")}});
        Project project = *f->model.project(QStringLiteral("p1"));
        project.moduleSettings.insert(QStringLiteral("test"), QJsonObject{{QStringLiteral("project"), QStringLiteral("a/b")}});
        f->model.setProject(project);

        f->controller.mode = Fetch::Hold;
        f->cache->refresh(QStringLiteral("i1"));
        QCOMPARE(f->controller.held.size(), 1);
        StatusJob *job = f->controller.held.first().second;
        QCOMPARE(job->config().value(QStringLiteral("name")).toString(), QStringLiteral("i1"));
        QCOMPARE(job->projectSetting(QStringLiteral("project")).toString(), QStringLiteral("a/b"));
        QCOMPARE(job->moduleSettings(QStringLiteral("general")).value(QStringLiteral("baseUrl")).toString(),
                 QStringLiteral("https://example.com"));
        QVERIFY(job->owner());
        job->finish(ItemStatus());
    }

    void removalEmptiesEntryAndSeenKeys()
    {
        f->controller.next[QStringLiteral("i4")] = okStatus(QStringLiteral("Open"));
        f->controller.next[QStringLiteral("i7")] = okStatus(QStringLiteral("Open"));
        f->cache->poll();
        ItemStatusSource &source = f->cache->source(QStringLiteral("i4"));
        f->device->setSeenAttention(QStringLiteral("ws1"), QStringLiteral("i4"), {QStringLiteral("approved")});
        f->device->setSeenAttention(QStringLiteral("ws1"), QStringLiteral("i7"), {QStringLiteral("approved")});

        QSignalSpy changed(f->cache.get(), &StatusCache::statusChanged);
        Task task = *f->model.task(QStringLiteral("t2"));
        task.items.clear();
        f->model.setTask(task);
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::None);
        QCOMPARE(source.current().state, ItemStatus::State::None);
        QVERIFY(!f->device->hasSeenAttention(QStringLiteral("ws1"), QStringLiteral("i4")));
        QVERIFY(changed.contains(QVariantList{QStringLiteral("i4")}));

        // Removing the task removes its items too.
        f->model.removeTask(QStringLiteral("t5"));
        QCOMPARE(f->cache->status(QStringLiteral("i7")).state, ItemStatus::State::None);
        QVERIFY(!f->device->hasSeenAttention(QStringLiteral("ws1"), QStringLiteral("i7")));
    }

    void subscribersGetEachStatus()
    {
        auto receiver = std::make_unique<QObject>();
        QStringList seen;
        f->cache->source(QStringLiteral("i4")).subscribe(receiver.get(), [&seen](const ItemStatus &status) {
            seen << status.summary;
        });
        f->controller.next[QStringLiteral("i4")] = okStatus(QStringLiteral("one"));
        f->cache->poll();
        f->controller.next[QStringLiteral("i4")] = okStatus(QStringLiteral("two"));
        f->cache->refresh(QStringLiteral("i4"));
        QCOMPARE(seen, QStringList({QStringLiteral("one"), QStringLiteral("two")}));

        receiver.reset();
        f->cache->refresh(QStringLiteral("i4"));
        QCOMPARE(seen.size(), 2);
    }

    void clearDropsEntries()
    {
        f->controller.next[QStringLiteral("i4")] = okStatus(QStringLiteral("Open"));
        f->cache->noteTaskOpened(QStringLiteral("t1"));
        f->cache->poll();
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::Ok);
        f->cache->clear();
        QCOMPARE(f->cache->status(QStringLiteral("i4")).state, ItemStatus::State::None);

        // The opened task is forgotten, but Urgent and High tasks stay due.
        takeFetched();
        f->cache->poll();
        QStringList fetched = takeFetched();
        fetched.sort();
        QCOMPARE(fetched, QStringList({QStringLiteral("i4"), QStringLiteral("i7")}));
    }

    // Another workspace loaded keeps the seen keys of the old one.
    void resetKeepsSeenKeys()
    {
        f->device->setSeenAttention(QStringLiteral("ws1"), QStringLiteral("i4"), {QStringLiteral("approved")});
        f->model.reset({}, {}, {}, {});
        QVERIFY(f->device->hasSeenAttention(QStringLiteral("ws1"), QStringLiteral("i4")));
    }
    // A job gives the local paths of the project's folders, and the main
    // folder's for an empty id.
    void jobReadsFolderPaths()
    {
        addFolders();
        f->device->setFolderPath(QStringLiteral("f1"), QStringLiteral("C:/main"));
        f->device->setFolderPath(QStringLiteral("f2"), QStringLiteral("C:/other"));
        f->controller.mode = Fetch::Hold;
        f->cache->refresh(QStringLiteral("i1"));
        QCOMPARE(f->controller.held.size(), 1);
        StatusJob *job = f->controller.held.first().second;
        QCOMPARE(job->folderPath(QString()), QStringLiteral("C:/main"));
        QCOMPARE(job->folderPath(QStringLiteral("f1")), QStringLiteral("C:/main"));
        QCOMPARE(job->folderPath(QStringLiteral("f2")), QStringLiteral("C:/other"));
        QVERIFY(job->folderPath(QStringLiteral("f3")).isEmpty());
        QVERIFY(job->gitHead(QString()).isEmpty());
        job->finish(ItemStatus());
    }

    // A new folder mapping fetches the project's items again when the cache
    // next looks at the model, as the project dialog's change does.
    void folderMappingFetchesAgain()
    {
        addFolders();
        f->cache->poll();
        takeFetched();
        f->device->setFolderPath(QStringLiteral("f1"), QStringLiteral("C:/main"));
        Project project = *f->model.project(QStringLiteral("p1"));
        project.name = QStringLiteral("Renamed");
        f->model.setProject(project);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i4")}));

        // A folder of another project fetches that project's items only.
        Project other = *f->model.project(QStringLiteral("p2"));
        Folder folder;
        folder.id = QStringLiteral("f9");
        folder.main = true;
        other.folders = {folder};
        f->model.setProject(other);
        takeFetched();
        f->device->setFolderPath(QStringLiteral("f9"), QStringLiteral("C:/p2"));
        project.name = QStringLiteral("Renamed again");
        f->model.setProject(project);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i7")}));
    }

    // A job that read a head is fetched again when the head changes, and the
    // watch survives Git replacing the file. A head no job read is not
    // watched. See decision 0080.
    void headChangeFetchesAgain()
    {
        QTemporaryDir repos;
        const QString read = repos.filePath(QStringLiteral("read"));
        const QString other = repos.filePath(QStringLiteral("other"));
        QVERIFY(makeRepository(read, QStringLiteral("main")));
        QVERIFY(makeRepository(other, QStringLiteral("main")));
        if (git::headFile(read).isEmpty())
            QSKIP("GitHead is not implemented in this build yet");

        f->controller.mode = Fetch::Hold;
        f->cache->setVisible(QStringLiteral("i1"), true);
        QCOMPARE(takeFetched(), QStringList({QStringLiteral("i1")}));
        QCOMPARE(runningJob(QStringLiteral("i1"))->gitHead(read), QStringLiteral("main"));
        finishHeld(QStringLiteral("i1"), read);

        QVERIFY(switchHead(read, QStringLiteral("feature"), QStringLiteral("i1")));
        QCOMPARE(runningJob(QStringLiteral("i1"))->gitHead(read), QStringLiteral("feature"));
        finishHeld(QStringLiteral("i1"), read);

        // Watched again after Git replaced the file.
        QVERIFY(switchHead(read, QStringLiteral("main"), QStringLiteral("i1")));
        finishHeld(QStringLiteral("i1"), read);

        // Another repository's head fetches nothing.
        QVERIFY(writeHead(other, QStringLiteral("feature")));
        QTest::qWait(500);
        QVERIFY(takeFetched().isEmpty());

        // A job that no longer reads the head stops the watch.
        f->cache->refresh(QStringLiteral("i1"));
        takeFetched();
        finishHeld(QStringLiteral("i1"), QString());
        QVERIFY(writeHead(read, QStringLiteral("feature")));
        QTest::qWait(500);
        QVERIFY(takeFetched().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestStatusCache)
#include "tst_status_cache.moc"
