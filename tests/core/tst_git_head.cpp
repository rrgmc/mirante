// Tests GitHead: a branch, a detached head, a worktree's .git file with an
// absolute and a relative gitdir, and folders without a head.
#include "core/GitHead.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace mirante;

namespace {

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

// Makes <path>/.git/HEAD with content.
bool makeRepository(const QString &path, const QByteArray &head)
{
    return QDir().mkpath(path + QStringLiteral("/.git")) && writeFile(path + QStringLiteral("/.git/HEAD"), head);
}

} // namespace

class TestGitHead : public QObject
{
    Q_OBJECT

private slots:
    void branch()
    {
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path(), "ref: refs/heads/feature/abc-123\n"));
        QCOMPARE(git::branchAt(dir.path()), QStringLiteral("feature/abc-123"));
        QCOMPARE(git::headFile(dir.path()), dir.path() + QStringLiteral("/.git/HEAD"));
    }

    void branchWithoutNewline()
    {
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path(), "ref: refs/heads/main"));
        QCOMPARE(git::branchAt(dir.path()), QStringLiteral("main"));
    }

    void detached()
    {
        QTemporaryDir dir;
        QVERIFY(makeRepository(dir.path(), "3f1c2a7d9e015d2a1b9e8c3f4e6ab7d01a2b3c4d\n"));
        QCOMPARE(git::branchAt(dir.path()), QString());
        QCOMPARE(git::headFile(dir.path()), dir.path() + QStringLiteral("/.git/HEAD"));
    }

    void worktreeAbsolute()
    {
        QTemporaryDir dir;
        const QString gitDir = dir.path() + QStringLiteral("/main/.git/worktrees/side");
        QVERIFY(QDir().mkpath(gitDir));
        QVERIFY(writeFile(gitDir + QStringLiteral("/HEAD"), "ref: refs/heads/side\n"));
        const QString work = dir.path() + QStringLiteral("/side");
        QVERIFY(QDir().mkpath(work));
        QVERIFY(writeFile(work + QStringLiteral("/.git"), "gitdir: " + gitDir.toUtf8() + "\n"));

        QCOMPARE(git::branchAt(work), QStringLiteral("side"));
        QCOMPARE(git::headFile(work), gitDir + QStringLiteral("/HEAD"));
    }

    // A submodule's .git file names its folder relative to the submodule.
    void worktreeRelative()
    {
        QTemporaryDir dir;
        const QString gitDir = dir.path() + QStringLiteral("/.git/modules/lib");
        QVERIFY(QDir().mkpath(gitDir));
        QVERIFY(writeFile(gitDir + QStringLiteral("/HEAD"), "ref: refs/heads/stable\n"));
        const QString work = dir.path() + QStringLiteral("/lib");
        QVERIFY(QDir().mkpath(work));
        QVERIFY(writeFile(work + QStringLiteral("/.git"), "gitdir: ../.git/modules/lib\n"));

        QCOMPARE(git::branchAt(work), QStringLiteral("stable"));
        QCOMPARE(git::headFile(work), gitDir + QStringLiteral("/HEAD"));
    }

    void missing()
    {
        QTemporaryDir dir;
        QCOMPARE(git::branchAt(dir.path()), QString());
        QCOMPARE(git::headFile(dir.path()), QString());
        QCOMPARE(git::branchAt(dir.path() + QStringLiteral("/nothing")), QString());
        QCOMPARE(git::branchAt(QString()), QString());
        QCOMPARE(git::headFile(QString()), QString());
    }

    // A .git folder without HEAD, and a .git file that names no folder.
    void unreadable()
    {
        QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/a/.git")));
        QCOMPARE(git::headFile(dir.path() + QStringLiteral("/a")), QString());

        QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/b")));
        QVERIFY(writeFile(dir.path() + QStringLiteral("/b/.git"), "not a gitdir line\n"));
        QCOMPARE(git::branchAt(dir.path() + QStringLiteral("/b")), QString());
        QCOMPARE(git::headFile(dir.path() + QStringLiteral("/b")), QString());
    }
};

QTEST_GUILESS_MAIN(TestGitHead)
#include "tst_git_head.moc"
