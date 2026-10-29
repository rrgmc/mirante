#pragma once

#include <QObject>
#include <QString>

#include <vector>

class QWidget;

namespace mirante {

// Follows the mouse over dashboard cells and says what the innermost hovered
// cell shows: the id of an item, or of a task for a task cell. The sidebar
// tints that row. Cells nest: an item cell inside a task cell wins while the
// mouse is over it, and the task cell takes over again when the mouse leaves
// the item cell but stays in the task cell. See docs/ui.md#sidebar-tree.
// Implemented in CellHoverTracker.cpp.
class CellHoverTracker : public QObject
{
    Q_OBJECT

public:
    explicit CellHoverTracker(QObject *parent = nullptr);
    ~CellHoverTracker() override;

    // Reports the mouse entering and leaving cell, which shows id. A deleted
    // cell stops counting as hovered.
    void watch(QWidget *cell, const QString &id);

    // The id of the innermost hovered cell, or empty.
    QString hoveredId() const;

signals:
    void hoveredChanged(const QString &id);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void enter(QObject *cell);
    void leave(QObject *cell);
    void update();

    struct Watched
    {
        QObject *cell;
        QString id;
    };
    std::vector<Watched> m_watched;
    // The hovered cells, outermost first. Qt sends Enter to a parent before
    // its children, and Leave to a child before its parent.
    std::vector<QObject *> m_hovered;
    QString m_current;
};

} // namespace mirante
