#include "layout/DashboardCellFactory.h"

#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/TaskCell.h"

#include <QCoreApplication>

namespace mirante {

struct DashboardCellFactory::Private
{
    LayoutServices services;
    DashboardRef dashboard;

    // The item a reference names, or nullptr. A task dashboard holds items of
    // its own task. A root dashboard names the task in the reference.
    const Item *item(const CellRef &ref) const
    {
        if (!services.model || !ref.isItem())
            return nullptr;
        const QString taskId = dashboard.isRoot() ? ref.task : dashboard.taskId;
        return services.model->item(taskId, ref.item);
    }

    // "Project / Task" for an item reference in a root dashboard.
    QString origin(const CellRef &ref) const
    {
        const Task *task = services.model ? services.model->task(ref.task) : nullptr;
        if (!task)
            return QString();
        const Project *project = services.model->project(task->projectId);
        return project ? QCoreApplication::translate("DashboardCellFactory", "%1 / %2").arg(project->name, task->title) : task->title;
    }

    const ItemType *type(const Item &item) const
    {
        return services.types ? &services.types->type(item.type) : nullptr;
    }
};

DashboardCellFactory::DashboardCellFactory(const LayoutServices &services, const DashboardRef &dashboard)
    : d(std::make_unique<Private>(Private{services, dashboard}))
{
}

DashboardCellFactory::~DashboardCellFactory() = default;

QWidget *DashboardCellFactory::createCell(const std::optional<CellRef> &ref, bool removed, QWidget *parent)
{
    if (!ref)
        return new CellFrame(std::nullopt, false, nullptr, QString(), parent);
    if (removed)
        return new CellFrame(ref, true, nullptr, QString(), parent);
    if (d->dashboard.isRoot() && ref->isTask())
        return new TaskCell(d->services, d->dashboard, *ref, parent);

    const Item *item = d->item(*ref);
    if (!item)
        return new CellFrame(ref, true, nullptr, QString(), parent);
    auto *frame = new CellFrame(ref, false, d->type(*item), item->name, parent);
    // In a task dashboard every item is of that task. In a root dashboard an
    // item can come from anywhere, so its header names its project and task.
    if (d->dashboard.isRoot())
        frame->setContext(d->origin(*ref));
    return frame;
}

bool DashboardCellFactory::canReparent(const std::optional<CellRef> &ref) const
{
    if (!ref)
        return true;
    const Item *item = d->item(*ref);
    const ItemType *type = item ? d->type(*item) : nullptr;
    return type ? type->reparentSafe() : true;
}

} // namespace mirante
