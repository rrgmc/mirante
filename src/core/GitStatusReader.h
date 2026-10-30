#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace mirante {

// A folder's Git state. The core's copy of the module interface's GitState,
// because the core library sees Qt Core only; ItemContextImpl converts it.
struct GitStatus
{
    enum class Result { Ok, NotARepository, GitMissing, Failed };

    Result result = Result::Failed;
    QString branch;     // empty when detached
    QString commit;     // short hash, empty before the first commit
    bool hasUpstream = false;
    int ahead = 0;
    int behind = 0;
    int staged = 0;
    int changed = 0;
    int untracked = 0;
    int conflicts = 0;
    QString message;    // for Failed
};

// Runs `git -C <path> status --porcelain=v2 --branch` in the background. See
// docs/decisions/0080-git-read-by-the-core.md. One run per path at a time: a
// request for a path that is being read waits for that run's answer.
// Implemented in GitStatusReader.cpp.
class GitStatusReader : public QObject
{
    Q_OBJECT

public:
    explicit GitStatusReader(QObject *parent = nullptr);
    ~GitStatusReader() override;

    // The git program, "git" by default, found on the PATH. Tests set a name
    // that does not exist, to see GitMissing.
    void setProgram(const QString &program);
    // How long a run may take, 10 seconds by default. Then it fails.
    void setTimeout(int ms);

    // Calls done once with the folder's state, on this object's thread,
    // unless receiver is destroyed first.
    void read(const QString &path, QObject *receiver, std::function<void(const GitStatus &)> done);

    // Reads the output of git status --porcelain=v2 --branch. The result is
    // Ok; the caller sets other results from the exit code.
    static GitStatus parse(const QByteArray &output);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
