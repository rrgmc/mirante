#pragma once

#include <mirante/ApiClient.h>

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

class QObject;

namespace mirante {

// A live reason to act now, such as a failed pipeline. See
// docs/decisions/0061-attention-from-status-of-any-rendering.md.
struct AttentionReason
{
    QString key;                // stable while the reason is the same, such as "status:10001"
    QString text;               // such as "Pipeline failed"
    bool notify = false;        // also a desktop notification, from M8
    bool clearsOnView = false;  // an event: stops counting once the item was seen
};

// What the status cache knows about one item.
struct ItemStatus
{
    enum class State { None, Ok, NoServer, NoToken, Refused, NotFound, Error };

    State state = State::None;
    QString summary;            // one line, such as "Open · pipeline failed"
    QString message;            // why, for every state but Ok and None
    QJsonObject data;           // what the type's views need
    QList<AttentionReason> attention;
    std::optional<bool> closed; // the work is finished, such as a merged MR
    QDateTime updatedAt;        // when data last came from the server
};

// The state that matches a failed reply, with its message, for a type that
// has nothing to add.
inline ItemStatus statusFromReply(const ApiReply &reply)
{
    ItemStatus status;
    status.message = reply.message;
    switch (reply.error) {
    case ApiReply::Error::None:
        status.state = ItemStatus::State::Ok;
        status.message.clear();
        break;
    case ApiReply::Error::NoServer:
        status.state = ItemStatus::State::NoServer;
        break;
    case ApiReply::Error::NoToken:
        status.state = ItemStatus::State::NoToken;
        break;
    case ApiReply::Error::Refused:
    case ApiReply::Error::Paused:
        status.state = ItemStatus::State::Refused;
        break;
    case ApiReply::Error::NotFound:
        status.state = ItemStatus::State::NotFound;
        break;
    case ApiReply::Error::Forbidden:
    case ApiReply::Error::Http:
    case ApiReply::Error::Network:
        status.state = ItemStatus::State::Error;
        break;
    }
    return status;
}

// One fetch of one item's status. The status cache makes it and passes it to
// ItemType::fetchStatus(). See docs/integrations.md#polling-and-caching.
class StatusJob
{
public:
    virtual ~StatusJob() = default;

    // The item's configuration, its project's setting of the item's module,
    // and a screen of the module's settings in the item's profile.
    virtual QJsonObject config() const = 0;
    virtual QJsonValue projectSetting(const QString &key) const = 0;
    virtual QJsonObject moduleSettings(const QString &screenId) const = 0;

    // The module's server in the item's profile.
    virtual ApiClient &server() = 0;

    // The local path of a project folder on this device, or empty if it is
    // not mapped. An empty id means the project's main folder.
    virtual QString folderPath(const QString &folderId) const
    {
        Q_UNUSED(folderId)
        return {};
    }

    // The branch checked out in a folder, as ItemContext::gitHead() reads it.
    // The status cache watches the head file the job read, and fetches the
    // item again when it changes. See decision 0080.
    virtual QString gitHead(const QString &path)
    {
        Q_UNUSED(path)
        return {};
    }

    // Lives as long as the job: the receiver for its requests.
    virtual QObject *owner() = 0;

    // Ends the job. Call it exactly once. The job is deleted afterwards.
    virtual void finish(const ItemStatus &status) = 0;
};

// An item's entry in the status cache, for its views.
class ItemStatusSource
{
public:
    virtual ~ItemStatusSource() = default;

    virtual ItemStatus current() const = 0;
    // Fetches at once.
    virtual void refresh() = 0;
    // Calls changed with each new status until receiver is destroyed.
    virtual void subscribe(QObject *receiver, std::function<void(const ItemStatus &)> changed) = 0;
};

} // namespace mirante
