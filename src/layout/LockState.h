#pragma once

#include <QObject>

#include <memory>

namespace mirante {

// Which dashboards are unlocked for editing. Every dashboard starts locked.
// Kept in memory only. Implemented in LockState.cpp.
class LockState : public QObject
{
    Q_OBJECT

public:
    explicit LockState(QObject *parent = nullptr);
    ~LockState() override;

    bool isUnlocked(const QString &dashboardId) const;
    void setUnlocked(const QString &dashboardId, bool unlocked);

signals:
    void changed(const QString &dashboardId, bool unlocked);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
