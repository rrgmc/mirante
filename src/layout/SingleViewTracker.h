#pragma once

#include <QObject>

#include <memory>

namespace mirante {

// Remembers which cell shows each item whose type has singleView on. Other
// cells of the same item show "Shown in another cell".
// Implemented in SingleViewTracker.cpp.
class SingleViewTracker : public QObject
{
    Q_OBJECT

public:
    explicit SingleViewTracker(QObject *parent = nullptr);
    ~SingleViewTracker() override;

    // The cell that shows the item, or nullptr.
    QWidget *holder(const QString &itemId) const;

    // Makes cell the holder of the item, replacing any other holder.
    void setHolder(const QString &itemId, QWidget *cell);

    // Forgets every item the cell holds. Called when a cell is destroyed.
    void release(QWidget *cell);

signals:
    void holderChanged(const QString &itemId);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
