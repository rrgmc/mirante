#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"
#include "layout/LayoutServices.h"

#include <QWidget>

#include <memory>

namespace mirante {

class LayoutView;

// The cell of a task dashboard in a root dashboard: a thin header with the
// task and dashboard names, the lock button and an "open alone" button, over a
// nested LayoutView of that task dashboard. Edits in the nested view change the
// task dashboard. See docs/decisions/0011-root-cells-reference-task-dashboards.md.
// Implemented in TaskCell.cpp.
class TaskCell : public QWidget
{
    Q_OBJECT

public:
    // rootDashboard is the root dashboard that holds the cell. The cell shows
    // ref.dashboard, or the task's default dashboard if ref has none.
    TaskCell(const LayoutServices &services, const DashboardRef &rootDashboard, const CellRef &ref,
             QWidget *parent = nullptr);
    ~TaskCell() override;

    QString taskId() const;
    QString dashboardId() const;    // the task dashboard shown
    LayoutView *view() const;       // the nested view

    // The root dashboard's edit mode, which makes the header a drag handle.
    // The nested view's edit mode follows the task dashboard's own lock.
    void setEditMode(bool on);

signals:
    void menuRequested(const QPoint &globalPos);
    void dragStarted();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
