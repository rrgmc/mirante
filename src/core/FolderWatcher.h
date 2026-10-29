#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace mirante {

// Watches the data folder for changes made by others: another app instance or
// a sync tool. See docs/sync.md#reading-external-changes and the M3 plan,
// part 3. Implemented in FolderWatcher.cpp.
//
// It watches the top folder, for workspace.mirante, and the subfolders groups/,
// projects/, tasks/, dashboards/ and files/. Paths in signals are relative to
// the data folder, such as "tasks/<id>.json".
class FolderWatcher : public QObject
{
    Q_OBJECT

public:
    // Says whether a file's content is this app's own last write, so it is not
    // reported. FileWriter::isOwnWrite fits.
    using OwnWriteCheck = std::function<bool(const QString &relPath, const QByteArray &content)>;

    FolderWatcher(const QString &dir, OwnWriteCheck isOwnWrite, QObject *parent = nullptr);
    ~FolderWatcher() override;

    // The quiet time after the last change event before a scan. 300 ms by default.
    void setDelay(int ms);

    // Records the files present now, and starts watching. Changes before this
    // call are not reported, except conflict copies, which are reported at once.
    void start();
    void stop();

signals:
    // Added or modified files, not counting own writes, conflict copies and
    // temporary files.
    void filesChanged(const QStringList &relPaths);
    void filesRemoved(const QStringList &relPaths);
    // Every conflict copy present now, whenever that list changes.
    void conflictCopiesChanged(const QStringList &relPaths);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
