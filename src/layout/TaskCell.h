#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"
#include "layout/LayoutServices.h"

#include <QWidget>

#include <memory>

namespace mirante {

class LayoutView;

// The cell of a task dashboard in a root dashboard: a thin header with the
// task and dashboard names and an "edit task dashboard" button, over a nested,
// read-only LayoutView of that task dashboard. A task dashboard is edited on
// its own: a double click on the cell, the button or the cell's menu opens it. Edits in the nested view change the
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
    // The nested view is never in edit mode.
    void setEditMode(bool on);

    // Opens the task dashboard on its own, where it can be edited.
    void openAlone();

signals:
    void menuRequested(const QPoint &globalPos);
    void dragStarted();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
