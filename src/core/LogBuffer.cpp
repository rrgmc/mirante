#include "core/LogBuffer.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>

#include <cstdio>
#include <deque>

namespace mirante {

namespace {

// The installed buffer and the handler it replaced. A message can arrive on
// any thread, so both are read under the mutex.
QMutex installMutex;
QPointer<LogBuffer> installed;
QtMessageHandler previousHandler = nullptr;

// Set while a message is being added, so a message logged by the code that
// shows it, such as the log window, does not loop back into the buffer.
thread_local bool adding = false;

void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QtMessageHandler previous = nullptr;
    LogBuffer *buffer = nullptr;
    {
        QMutexLocker lock(&installMutex);
        previous = previousHandler;
        buffer = installed.data();
    }
    if (buffer && !adding) {
        adding = true;
        buffer->add(LogEntry{QDateTime::currentDateTime(), type,
                             QString::fromUtf8(context.category ? context.category : ""), message});
        adding = false;
    }
    if (previous) {
        previous(type, context, message);
    } else {
        // Qt's own handler is not public; this prints what it would.
        fprintf(stderr, "%s\n", qPrintable(qFormatLogMessage(type, context, message)));
        fflush(stderr);
    }
}

} // namespace

struct LogBuffer::Private
{
    int capacity = 5000;
    mutable QMutex mutex;
    std::deque<LogEntry> entries;
};

LogBuffer::LogBuffer(int capacity, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->capacity = qMax(1, capacity);
    qRegisterMetaType<mirante::LogEntry>();
}

LogBuffer::~LogBuffer()
{
    QMutexLocker lock(&installMutex);
    if (installed == this) {
        qInstallMessageHandler(previousHandler);
        installed = nullptr;
        previousHandler = nullptr;
    }
}

void LogBuffer::install()
{
    QMutexLocker lock(&installMutex);
    if (installed == this)
        return;
    QtMessageHandler previous = qInstallMessageHandler(handleMessage);
    // Replacing another buffer keeps the handler that buffer replaced.
    if (!installed)
        previousHandler = previous;
    installed = this;
}

LogBuffer *LogBuffer::installedBuffer()
{
    QMutexLocker lock(&installMutex);
    return installed.data();
}

std::vector<LogEntry> LogBuffer::entries() const
{
    QMutexLocker lock(&d->mutex);
    return {d->entries.begin(), d->entries.end()};
}

void LogBuffer::clear()
{
    {
        QMutexLocker lock(&d->mutex);
        d->entries.clear();
    }
    emit cleared();
}

void LogBuffer::add(const LogEntry &entry)
{
    {
        QMutexLocker lock(&d->mutex);
        d->entries.push_back(entry);
        while (int(d->entries.size()) > d->capacity)
            d->entries.pop_front();
    }
    // The window lives on the main thread, so a message from another thread
    // reaches it through the event loop.
    QMetaObject::invokeMethod(this, [this, entry] { emit entryAdded(entry); }, Qt::AutoConnection);
}

} // namespace mirante
