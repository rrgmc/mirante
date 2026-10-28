#pragma once

#include "core/LayoutTree.h"
#include "core/Model.h"
#include "layout/LayoutServices.h"

#include <QWidget>

#include <memory>

namespace mirante {

class LayoutView;

// The cell of a task in a root dashboard: a thin header with the task name, a
// dashboard picker, the lock button and an "open alone" button, over a nested
// LayoutView of the chosen task dashboard. Edits in the nested view change the
// task dashboard. See docs/decisions/0011-root-cells-reference-task-dashboards.md.
// Implemented in TaskCell.cpp.
class TaskCell : public QWidget
{
    Q_OBJECT

public:
    // rootDashboard is the root dashboard that holds the cell. Choosing another
    // task dashboard updates the cell's reference in it.
    TaskCell(const LayoutServices &services, const DashboardRef &rootDashboard, const CellRef &ref,
             QWidget *parent = nullptr);
    ~TaskCell() override;

    QString taskId() const;
    QString dashboardId() const;    // the chosen task dashboard
    LayoutView *view() const;       // the nested view

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
