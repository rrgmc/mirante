#include "core/GitHead.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace mirante::git {

namespace {

// Head files are one short line; anything longer is not one.
constexpr qint64 kMaxRead = 4096;

// The first line of a file, trimmed, or empty if it cannot be read.
QString firstLine(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray content = file.read(kMaxRead);
    const qsizetype end = content.indexOf('\n');
    return QString::fromUtf8(end < 0 ? content : content.left(end)).trimmed();
}

// The Git folder of path: <path>/.git, or the folder named by the "gitdir:"
// line when .git is a file, as in a worktree or a submodule.
QString gitDir(const QString &path)
{
    if (path.isEmpty())
        return {};
    const QString dotGit = QDir(path).filePath(QStringLiteral(".git"));
    const QFileInfo info(dotGit);
    if (info.isDir())
        return dotGit;
    if (!info.isFile())
        return {};
    const QString line = firstLine(dotGit);
    const QString prefix = QStringLiteral("gitdir:");
    if (!line.startsWith(prefix))
        return {};
    const QString target = line.mid(prefix.size()).trimmed();
    if (target.isEmpty())
        return {};
    return QDir::cleanPath(QDir(path).absoluteFilePath(target));
}

} // namespace

QString branchAt(const QString &path)
{
    const QString head = headFile(path);
    if (head.isEmpty())
        return {};
    const QString line = firstLine(head);
    const QString prefix = QStringLiteral("ref: refs/heads/");
    if (!line.startsWith(prefix))
        return {};  // detached: a commit hash
    return line.mid(prefix.size()).trimmed();
}

QString headFile(const QString &path)
{
    const QString dir = gitDir(path);
    if (dir.isEmpty())
        return {};
    const QString head = QDir(dir).filePath(QStringLiteral("HEAD"));
    return QFileInfo(head).isFile() ? head : QString();
}

} // namespace mirante::git
