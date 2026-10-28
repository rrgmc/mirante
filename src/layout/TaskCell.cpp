// Stub from task T0. Task T14 implements this file.
#include "layout/TaskCell.h"

namespace mirante {

struct TaskCell::Private
{
    CellRef ref;
};

TaskCell::TaskCell(const LayoutServices & /*services*/, const DashboardRef & /*rootDashboard*/, const CellRef &ref,
                   QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->ref = ref;
}

TaskCell::~TaskCell() = default;

QString TaskCell::taskId() const { return d->ref.task; }
QString TaskCell::dashboardId() const { return d->ref.dashboard; }
LayoutView *TaskCell::view() const { return nullptr; }

} // namespace mirante
