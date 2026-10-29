#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>
#include <optional>

namespace mirante {

// Writes files in the data folder with QSaveFile, debounced, and remembers
// what it wrote so its own writes can be told from other people's. See
// docs/sync.md#writing and the M3 plan, part 3. Implemented in FileWriter.cpp.
class FileWriter : public QObject
{
    Q_OBJECT

public:
    explicit FileWriter(const QString &dir, QObject *parent = nullptr);
    ~FileWriter() override;

    // The quiet time before pending writes happen. 500 ms by default; 0 writes
    // on the next turn of the event loop.
    void setDelay(int ms);

    // Schedules a write of relPath, such as "tasks/<id>.json". nullopt deletes
    // the file. A later call for the same path replaces an earlier one that has
    // not been written yet. Missing subfolders are created.
    void schedule(const QString &relPath, std::optional<QByteArray> content);

    // Writes everything pending now.
    void flush();
    bool pending() const;

    // Whether content is exactly what this writer last wrote to relPath. For a
    // file it deleted, an empty content matches.
    bool isOwnWrite(const QString &relPath, const QByteArray &content) const;

signals:
    // A write that failed stays pending and is tried again with the next flush.
    // It is reported once until it succeeds.
    void writeFailed(const QString &relPath, const QString &message);
    void written(const QString &relPath);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
