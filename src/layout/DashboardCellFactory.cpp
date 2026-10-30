#include "layout/DashboardCellFactory.h"

#include "core/GroupColours.h"
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "layout/CellHoverTracker.h"
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

    // "Project › Task" for an item reference in a root dashboard.
    QString origin(const CellRef &ref) const
    {
        const Task *task = services.model ? services.model->task(ref.task) : nullptr;
        if (!task)
            return QString();
        const Project *project = services.model->project(task->projectId);
        return project ? QCoreApplication::translate("DashboardCellFactory", "%1 › %2").arg(project->name, task->title) : task->title;
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
    // Normalisation drops the reference of a removed cell, so removed is
    // checked first.
    if (removed)
        return new CellFrame(ref, true, nullptr, QString(), parent);
    if (!ref)
        return new CellFrame(std::nullopt, false, nullptr, QString(), parent);
    if (d->dashboard.isRoot() && ref->isTask()) {
        auto *cell = new TaskCell(d->services, d->dashboard, *ref, parent);
        if (d->services.hover)
            d->services.hover->watch(cell, ref->task);
        return cell;
    }

    const Item *item = d->item(*ref);
    if (!item)
        return new CellFrame(ref, true, nullptr, QString(), parent);
    // An item of a task in the trash keeps its reference, so a restore brings it back.
    if (d->services.model->taskInTrash(d->dashboard.isRoot() ? ref->task : d->dashboard.taskId)) {
        auto *trashed = new CellFrame(ref, true, d->type(*item), item->name, parent);
        trashed->setMessage(QCoreApplication::translate("DashboardCellFactory", "In trash"));
        return trashed;
    }
    auto *frame = new CellFrame(ref, false, d->type(*item), item->name, parent);
    // In a task dashboard every item is of that task. In a root dashboard an
    // item can come from anywhere, so its header names its project and task.
    if (d->dashboard.isRoot())
        frame->setContext(d->origin(*ref));
    if (d->services.hover)
        d->services.hover->watch(frame, ref->item);

    // The header takes a light tint of the group colour of the item's task,
    // and follows changes to the task, its project and its group. See
    // docs/decisions/0047-item-headers-in-group-colours.md.
    {
        Model *model = d->services.model;
        const QString taskId = d->dashboard.isRoot() ? ref->task : d->dashboard.taskId;
        const auto tint = [model, taskId, frame] {
            const QString colour = groupcolours::ofTask(*model, taskId);
            frame->setTint(colour.isEmpty() ? QColor() : QColor(colour));
        };
        tint();
        QObject::connect(model, &Model::taskChanged, frame, tint);
        QObject::connect(model, &Model::projectChanged, frame, tint);
        QObject::connect(model, &Model::groupChanged, frame, tint);
        QObject::connect(model, &Model::modelReset, frame, tint);
    }

    // The item's view, made when the cell is first shown.
    if (d->services.placements) {
        const QString taskId = d->dashboard.isRoot() ? ref->task : d->dashboard.taskId;
        auto *placement = new Placement(*d->services.placements, taskId, ref->item, frame);
        QObject::connect(placement, &Placement::headerWidgetChanged, frame, &CellFrame::setHeaderWidget);
        // The item's status in the header. A change of the item's type can
        // add or remove its status, and with it the refresh button.
        frame->setStatus(placement->cellStatus());
        frame->setRefreshVisible(placement->hasStatus());
        QObject::connect(placement, &Placement::cellStatusChanged, frame, [frame, placement](const CellStatus &status) {
            frame->setStatus(status);
            frame->setRefreshVisible(placement->hasStatus());
        });
        QObject::connect(frame, &CellFrame::refreshRequested, placement, &Placement::refresh);
        QObject::connect(frame, &CellFrame::fixRequested, placement, &Placement::openSettings);
        frame->setBody(placement);
    }
    // The callback is a copy, so it stays valid if the factory goes first.
    if (const auto toggle = d->services.toggleMaximised) {
        frame->setMaximiseButtonVisible(true);
        QObject::connect(frame, &CellFrame::maximiseRequested, frame, [toggle, frame] { toggle(frame); });
    }
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
