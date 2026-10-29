#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <memory>
#include <vector>

namespace mirante {

// One message from qDebug(), qInfo(), qWarning() and the like, including the
// JavaScript console of web pages, which Qt logs in the "js" category.
struct LogEntry
{
    QDateTime time;
    QtMsgType type = QtDebugMsg;
    QString category;
    QString message;
};

// Keeps the latest Qt log messages for the log window of debug builds. It
// installs itself as Qt's message handler and passes every message on to the
// handler that was there before, so the console output does not change.
// Implemented in LogBuffer.cpp.
class LogBuffer : public QObject
{
    Q_OBJECT

public:
    // Keeps at most capacity messages, dropping the oldest.
    explicit LogBuffer(int capacity = 5000, QObject *parent = nullptr);
    // Restores the previous handler if this buffer is installed.
    ~LogBuffer() override;

    // Makes this buffer Qt's message handler. At most one buffer is installed
    // at a time; installing another replaces it.
    void install();

    // The installed buffer, or null.
    static LogBuffer *installedBuffer();

    std::vector<LogEntry> entries() const;
    void clear();

    // Adds a message. The message handler calls it from any thread; tests may
    // call it directly.
    void add(const LogEntry &entry);

signals:
    // Emitted on the buffer's thread, after the entry is stored.
    void entryAdded(const mirante::LogEntry &entry);
    void cleared();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
