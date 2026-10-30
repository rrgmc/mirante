// Tests GitStatusReader: parsing recorded porcelain v2 output, and runs of
// the real git in repositories made in a temporary folder, skipped when Git
// is not installed. Also Git missing, a folder that is not a repository, a
// time limit, one run per path, and receivers destroyed before the answer.
#include "core/GitStatusReader.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

using namespace mirante;

namespace {

constexpr int kTimeout = 20000;

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

// Runs git in dir, without the user's signing or hooks getting in the way.
bool git(const QString &dir, const QStringList &arguments)
{
    QProcess process;
    process.setWorkingDirectory(dir);
    process.start(QStringLiteral("git"), QStringList{QStringLiteral("-c"), QStringLiteral("user.name=Test"),
                                                     QStringLiteral("-c"), QStringLiteral("user.email=test@example.com"),
                                                     QStringLiteral("-c"), QStringLiteral("commit.gpgsign=false"),
                                                     QStringLiteral("-c"), QStringLiteral("core.hooksPath=")}
                                             + arguments);
    return process.waitForFinished(kTimeout) && process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
}

// A repository on branch main with one commit of a.txt.
bool makeRepository(const QString &dir)
{
    return git(dir, {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main")})
        && writeFile(dir + QStringLiteral("/a.txt"), "one\n")
        && git(dir, {QStringLiteral("add"), QStringLiteral("a.txt")})
        && git(dir, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("first")});
}

} // namespace

class TestGitStatus : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Keeps git from finding a repository above the temporary folders.
        qputenv("GIT_CEILING_DIRECTORIES", QDir::tempPath().toUtf8());
    }

    // --- parse ---

    void parseClean()
    {
        const GitStatus status = GitStatusReader::parse("# branch.oid 3f1c2a7d9e015d2a1b9e8c3f4e6ab7d01a2b3c4d\n"
                                                        "# branch.head main\n"
                                                        "# branch.upstream origin/main\n"
                                                        "# branch.ab +0 -0\n");
        QCOMPARE(status.result, GitStatus::Result::Ok);
        QCOMPARE(status.branch, QStringLiteral("main"));
        QCOMPARE(status.commit, QStringLiteral("3f1c2a7"));
        QVERIFY(status.hasUpstream);
        QCOMPARE(status.ahead, 0);
        QCOMPARE(status.behind, 0);
        QCOMPARE(status.staged + status.changed + status.untracked + status.conflicts, 0);
    }

    void parseAheadBehind()
    {
        const GitStatus status = GitStatusReader::parse("# branch.oid 3f1c2a7d9e015d2a1b9e8c3f4e6ab7d01a2b3c4d\n"
                                                        "# branch.head feature/x\n"
                                                        "# branch.upstream origin/feature/x\n"
                                                        "# branch.ab +2 -13\n");
        QCOMPARE(status.branch, QStringLiteral("feature/x"));
        QVERIFY(status.hasUpstream);
        QCOMPARE(status.ahead, 2);
        QCOMPARE(status.behind, 13);
    }

    void parseNoUpstream()
    {
        const GitStatus status = GitStatusReader::parse("# branch.oid 3f1c2a7d9e015d2a1b9e8c3f4e6ab7d01a2b3c4d\n"
                                                        "# branch.head local\n");
        QCOMPARE(status.branch, QStringLiteral("local"));
        QVERIFY(!status.hasUpstream);
        QCOMPARE(status.ahead, 0);
        QCOMPARE(status.behind, 0);
    }

    void parseDetached()
    {
        const GitStatus status = GitStatusReader::parse("# branch.oid 3f1c2a7d9e015d2a1b9e8c3f4e6ab7d01a2b3c4d\n"
                                                        "# branch.head (detached)\n");
        QCOMPARE(status.branch, QString());
        QCOMPARE(status.commit, QStringLiteral("3f1c2a7"));
    }

    void parseInitial()
    {
        const GitStatus status = GitStatusReader::parse("# branch.oid (initial)\n"
                                                        "# branch.head main\n"
                                                        "? new.txt\n");
        QCOMPARE(status.branch, QStringLiteral("main"));
        QCOMPARE(status.commit, QString());
        QCOMPARE(status.untracked, 1);
    }

    // Staged in the index (X), changed in the working tree (Y), both, a
    // rename, a conflict and untracked files. Ignored files do not count.
    void parseCounts()
    {
        const GitStatus status = GitStatusReader::parse(
            "# branch.oid 3f1c2a7d9e015d2a1b9e8c3f4e6ab7d01a2b3c4d\n"
            "# branch.head main\n"
            "1 M. N... 100644 100644 100644 aaaaaaa bbbbbbb staged.txt\n"
            "1 .M N... 100644 100644 100644 aaaaaaa aaaaaaa changed.txt\n"
            "1 MM N... 100644 100644 100644 aaaaaaa bbbbbbb both.txt\n"
            "1 A. N... 000000 100644 100644 0000000 bbbbbbb added.txt\n"
            "1 .D N... 100644 100644 000000 aaaaaaa aaaaaaa deleted.txt\n"
            "2 R. N... 100644 100644 100644 aaaaaaa aaaaaaa R100 new name.txt\told name.txt\n"
            "u UU N... 100644 100644 100644 100644 aaaaaaa bbbbbbb ccccccc conflict.txt\n"
            "? untracked one.txt\n"
            "? untracked-two.txt\n"
            "! ignored.log\n");
        QCOMPARE(status.staged, 4);
        QCOMPARE(status.changed, 3);
        QCOMPARE(status.conflicts, 1);
        QCOMPARE(status.untracked, 2);
    }

    void parseWindowsLineEnds()
    {
        const GitStatus status = GitStatusReader::parse("# branch.head main\r\n# branch.ab +1 -0\r\n? x\r\n");
        QCOMPARE(status.branch, QStringLiteral("main"));
        QCOMPARE(status.ahead, 1);
        QCOMPARE(status.untracked, 1);
    }

    // --- runs ---

    void gitMissing()
    {
        GitStatusReader reader;
        reader.setProgram(QStringLiteral("mirante-no-such-git-program"));
        std::optional<GitStatus> answer;
        reader.read(QDir::tempPath(), this, [&](const GitStatus &status) { answer = status; });
        QVERIFY(!answer.has_value());   // never during read()
        QTRY_VERIFY_WITH_TIMEOUT(answer.has_value(), kTimeout);
        QCOMPARE(answer->result, GitStatus::Result::GitMissing);
    }

    void cleanRepository()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));

        const GitStatus status = readOnce(dir.path());
        QCOMPARE(status.result, GitStatus::Result::Ok);
        QCOMPARE(status.branch, QStringLiteral("main"));
        QCOMPARE(status.commit.size(), 7);
        QVERIFY(!status.hasUpstream);
        QCOMPARE(status.staged + status.changed + status.untracked + status.conflicts, 0);
    }

    void changedRepository()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));
        QVERIFY(writeFile(dir.path() + QStringLiteral("/a.txt"), "two\n"));
        QVERIFY(writeFile(dir.path() + QStringLiteral("/b.txt"), "new\n"));
        QVERIFY(git(dir.path(), {QStringLiteral("add"), QStringLiteral("b.txt")}));
        QVERIFY(writeFile(dir.path() + QStringLiteral("/c.txt"), "loose\n"));

        const GitStatus status = readOnce(dir.path());
        QCOMPARE(status.result, GitStatus::Result::Ok);
        QCOMPARE(status.staged, 1);
        QCOMPARE(status.changed, 1);
        QCOMPARE(status.untracked, 1);
        QCOMPARE(status.conflicts, 0);
    }

    void detachedRepository()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));
        QVERIFY(git(dir.path(), {QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("--detach")}));

        const GitStatus status = readOnce(dir.path());
        QCOMPARE(status.result, GitStatus::Result::Ok);
        QCOMPARE(status.branch, QString());
        QCOMPARE(status.commit.size(), 7);
    }

    void notARepository()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        const GitStatus status = readOnce(dir.path());
        QCOMPARE(status.result, GitStatus::Result::NotARepository);
    }

    void timeLimit()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));
        GitStatusReader reader;
        reader.setTimeout(0);
        std::optional<GitStatus> answer;
        reader.read(dir.path(), this, [&](const GitStatus &status) { answer = status; });
        QTRY_VERIFY_WITH_TIMEOUT(answer.has_value(), kTimeout);
        QCOMPARE(answer->result, GitStatus::Result::Failed);
        QVERIFY(answer->message.contains(QStringLiteral("longer than")));
    }

    // A second request for the same path waits for the first run, and both
    // answers arrive, in order. Another path gets its own run.
    void oneRunPerPath()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));
        QTemporaryDir other;
        QVERIFY(makeRepository(other.path()));

        GitStatusReader reader;
        QStringList order;
        reader.read(dir.path(), this, [&](const GitStatus &) { order << QStringLiteral("first"); });
        reader.read(dir.path(), this, [&](const GitStatus &) { order << QStringLiteral("second"); });
        QCOMPARE(reader.findChildren<QProcess *>().size(), 1);
        reader.read(other.path(), this, [&](const GitStatus &) { order << QStringLiteral("other"); });
        QCOMPARE(reader.findChildren<QProcess *>().size(), 2);

        QTRY_COMPARE_WITH_TIMEOUT(order.size(), 3, kTimeout);
        QVERIFY(order.indexOf(QStringLiteral("first")) < order.indexOf(QStringLiteral("second")));

        // Once answered, a new request starts a new run.
        bool again = false;
        reader.read(dir.path(), this, [&](const GitStatus &status) {
            again = status.result == GitStatus::Result::Ok;
        });
        QTRY_VERIFY_WITH_TIMEOUT(again, kTimeout);
    }

    void destroyedReceiverNotCalled()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));

        GitStatusReader reader;
        auto *receiver = new QObject;
        bool goneCalled = false;
        bool keptCalled = false;
        reader.read(dir.path(), receiver, [&](const GitStatus &) { goneCalled = true; });
        reader.read(dir.path(), this, [&](const GitStatus &) { keptCalled = true; });
        delete receiver;

        QTRY_VERIFY_WITH_TIMEOUT(keptCalled, kTimeout);
        QVERIFY(!goneCalled);
    }

    // A reader destroyed while git runs calls nobody, and does not crash.
    void destroyedWhileRunning()
    {
        if (!hasGit())
            QSKIP("Git is not on the PATH.");
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path()));
        bool called = false;
        {
            GitStatusReader reader;
            reader.read(dir.path(), this, [&](const GitStatus &) { called = true; });
        }
        QTest::qWait(200);
        QVERIFY(!called);
    }

private:
    static bool hasGit() { return !QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty(); }

    GitStatus readOnce(const QString &path)
    {
        GitStatusReader reader;
        std::optional<GitStatus> answer;
        reader.read(path, this, [&](const GitStatus &status) { answer = status; });
        if (!QTest::qWaitFor([&] { return answer.has_value(); }, kTimeout))
            return {};
        return *answer;
    }
};

QTEST_GUILESS_MAIN(TestGitStatus)
#include "tst_git_status.moc"
