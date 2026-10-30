#include "core/GitStatusReader.h"

#include <QCoreApplication>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

#include <map>
#include <vector>

namespace mirante {

namespace {

constexpr int kDefaultTimeout = 10000;

// One caller waiting for a run.
struct Waiter
{
    QPointer<QObject> receiver;
    bool hasReceiver = false;   // a null receiver means "always call"
    std::function<void(const GitStatus &)> done;
};

// One git process for one path, and everyone waiting for its answer.
struct Run
{
    QProcess *process = nullptr;
    QTimer *timer = nullptr;
    std::vector<Waiter> waiters;
};

GitStatus failure(GitStatus::Result result, const QString &message = {})
{
    GitStatus status;
    status.result = result;
    status.message = message;
    return status;
}

// The first non-empty line of git's error output, without "fatal: ".
QString errorLine(const QByteArray &error)
{
    for (const QByteArray &raw : error.split('\n')) {
        QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("fatal: ")))
            line = line.mid(7);
        return line;
    }
    return {};
}

int number(const QByteArray &text)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    return ok ? value : 0;
}

} // namespace

struct GitStatusReader::Private
{
    GitStatusReader *q;
    QString program = QStringLiteral("git");
    int timeout = kDefaultTimeout;
    std::map<QString, std::unique_ptr<Run>> runs;   // by path

    void start(const QString &path);
    void complete(const QString &path, QProcess *process, const GitStatus &status);
    void finished(const QString &path, QProcess *process, int exitCode, QProcess::ExitStatus exitStatus);
};

void GitStatusReader::Private::start(const QString &path)
{
    Run &run = *runs.at(path);
    run.process = new QProcess(q);
    run.timer = new QTimer(q);
    run.timer->setSingleShot(true);
    QProcess *process = run.process;

    // No index.lock taken by a background read, and git's messages in
    // English, so "not a git repository" can be recognised.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process->setProcessEnvironment(env);
    process->setProgram(program);
    process->setArguments({QStringLiteral("-C"), path, QStringLiteral("status"), QStringLiteral("--porcelain=v2"),
                           QStringLiteral("--branch")});

    QObject::connect(process, &QProcess::finished, q,
                     [this, path, process](int exitCode, QProcess::ExitStatus exitStatus) {
                         finished(path, process, exitCode, exitStatus);
                     });
    // Queued, because start() may report a missing program before it returns,
    // and read() calls back only later.
    QObject::connect(
        process, &QProcess::errorOccurred, q,
        [this, path, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                complete(path, process,
                         failure(GitStatus::Result::GitMissing,
                                 QCoreApplication::translate("GitStatusReader", "Git was not found.")));
        },
        Qt::QueuedConnection);
    const int seconds = timeout / 1000;
    QObject::connect(run.timer, &QTimer::timeout, q, [this, path, process, seconds] {
        complete(path, process,
                 failure(GitStatus::Result::Failed,
                         QCoreApplication::translate("GitStatusReader", "Git took longer than %1 seconds.")
                             .arg(qMax(seconds, 1))));
    });

    run.timer->start(timeout);
    process->start();
}

void GitStatusReader::Private::finished(const QString &path, QProcess *process, int exitCode,
                                        QProcess::ExitStatus exitStatus)
{
    if (exitStatus != QProcess::NormalExit) {
        complete(path, process,
                 failure(GitStatus::Result::Failed,
                         QCoreApplication::translate("GitStatusReader", "Git stopped unexpectedly.")));
        return;
    }
    if (exitCode == 0) {
        complete(path, process, GitStatusReader::parse(process->readAllStandardOutput()));
        return;
    }
    const QByteArray error = process->readAllStandardError();
    if (exitCode == 128 && error.contains("not a git repository")) {
        complete(path, process, failure(GitStatus::Result::NotARepository));
        return;
    }
    QString message = errorLine(error);
    if (message.isEmpty())
        message = QCoreApplication::translate("GitStatusReader", "Git failed with exit code %1.").arg(exitCode);
    complete(path, process, failure(GitStatus::Result::Failed, message));
}

void GitStatusReader::Private::complete(const QString &path, QProcess *process, const GitStatus &status)
{
    // A late signal from a process that already answered, or from an older
    // run of the same path, is ignored.
    const auto it = runs.find(path);
    if (it == runs.end() || it->second->process != process)
        return;
    std::unique_ptr<Run> run = std::move(it->second);
    runs.erase(it);

    run->timer->stop();
    run->timer->deleteLater();
    QObject::disconnect(process, nullptr, q, nullptr);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(1000);
    }
    process->deleteLater();

    for (const Waiter &waiter : run->waiters) {
        if (waiter.hasReceiver && waiter.receiver.isNull())
            continue;
        waiter.done(status);
    }
}

GitStatusReader::GitStatusReader(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
}

GitStatusReader::~GitStatusReader()
{
    // Stop the processes before d goes, so none reports into it.
    for (const auto &entry : d->runs) {
        QProcess *process = entry.second->process;
        QObject::disconnect(process, nullptr, this, nullptr);
        process->kill();
        process->waitForFinished(1000);
    }
}

void GitStatusReader::setProgram(const QString &program) { d->program = program; }

void GitStatusReader::setTimeout(int ms) { d->timeout = ms; }

void GitStatusReader::read(const QString &path, QObject *receiver, std::function<void(const GitStatus &)> done)
{
    Waiter waiter{receiver, receiver != nullptr, std::move(done)};
    const auto it = d->runs.find(path);
    if (it != d->runs.end()) {
        it->second->waiters.push_back(std::move(waiter));
        return;
    }
    auto run = std::make_unique<Run>();
    run->waiters.push_back(std::move(waiter));
    d->runs.emplace(path, std::move(run));
    d->start(path);
}

GitStatus GitStatusReader::parse(const QByteArray &output)
{
    GitStatus status;
    status.result = GitStatus::Result::Ok;

    for (const QByteArray &line : output.split('\n')) {
        if (line.startsWith("# branch.oid ")) {
            const QByteArray oid = line.mid(13).trimmed();
            if (oid != "(initial)")
                status.commit = QString::fromLatin1(oid.left(7));
        } else if (line.startsWith("# branch.head ")) {
            const QByteArray head = line.mid(14).trimmed();
            if (head != "(detached)")
                status.branch = QString::fromUtf8(head);
        } else if (line.startsWith("# branch.upstream ")) {
            status.hasUpstream = true;
        } else if (line.startsWith("# branch.ab ")) {
            // "# branch.ab +<ahead> -<behind>"
            const QList<QByteArray> parts = line.mid(12).trimmed().split(' ');
            for (const QByteArray &part : parts) {
                if (part.startsWith('+'))
                    status.ahead = number(part.mid(1));
                else if (part.startsWith('-'))
                    status.behind = number(part.mid(1));
            }
        } else if (line.startsWith("1 ") || line.startsWith("2 ")) {
            // "1 XY ...": X is the index, Y the working tree; '.' is unchanged.
            if (line.size() < 4)
                continue;
            if (line.at(2) != '.')
                ++status.staged;
            if (line.at(3) != '.')
                ++status.changed;
        } else if (line.startsWith("u ")) {
            ++status.conflicts;
        } else if (line.startsWith("? ")) {
            ++status.untracked;
        }
    }
    return status;
}

} // namespace mirante
