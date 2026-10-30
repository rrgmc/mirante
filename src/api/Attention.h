#pragma once

#include <mirante/Status.h>

#include <QList>
#include <QObject>
#include <QString>

#include <memory>

namespace mirante {

class DeviceSettings;
class Model;
class StatusCache;

// Turns the status cache's attention reasons into what the sidebar, Focus and
// cell headers show. A reason counts unless it clears on view and its key is
// among the item's seen keys, kept in DeviceSettings. An item's first status
// records its clearsOnView keys as seen. See the M5 plan, part 4, and
// docs/decisions/0061-attention-from-status-of-any-rendering.md.
// Implemented in Attention.cpp.
class Attention : public QObject
{
    Q_OBJECT

public:
    // Everything passed must outlive this object.
    Attention(const Model &model, StatusCache &status, DeviceSettings &settings, QObject *parent = nullptr);
    ~Attention() override;

    // The item's reasons that count, for tooltips.
    QList<AttentionReason> reasons(const QString &itemId) const;
    bool needsAttention(const QString &itemId) const;

    // The number of a task's items that need attention. Done tasks and tasks
    // in the trash count zero. A project and a group sum their active tasks.
    int taskCount(const QString &taskId) const;
    int projectCount(const QString &projectId) const;
    int groupCount(const QString &groupId) const;

    // The user looked at the item: its current clearsOnView reasons become
    // seen.
    void markSeen(const QString &itemId);

signals:
    // Some count may have changed. Emitted at most once per event loop pass.
    void countsChanged();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
