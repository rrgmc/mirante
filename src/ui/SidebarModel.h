#pragma once

#include "core/Sorting.h"

#include <QAbstractItemModel>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class Model;

// The sidebar tree over Model: groups, then projects, then tasks. Each task
// has two fixed children, "Dashboards" and "Items", which hold the task's
// task dashboards and items. Groups use their manual order; projects and tasks
// are sorted with Sorting. Items and dashboards are dragged onto dashboards as
// DragMime payloads. Groups, projects and tasks are dragged only to reorder the
// sidebar, under a sidebar-only MIME type.
// Implemented in SidebarModel.cpp.
class SidebarModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Kind { Group, Project, Task, ItemList, Item, DashboardList, Dashboard };

    enum Role {
        KindRole = Qt::UserRole + 1,    // Kind, as int
        IdRole,                         // the entity id
        TaskIdRole,                     // for a task and everything under it: the task id
        PriorityRole,                   // for projects and tasks: Priority, as int
        GroupColourRole,                // for groups and projects: the group's colour, as QColor
    };

    SidebarModel(const Model &model, const ItemTypeRegistry &types, QObject *parent = nullptr);
    ~SidebarModel() override;

    void setFilter(const SortFilter &filter);
    SortFilter filter() const;

    // The index of a group, project, task or item, or an invalid index.
    QModelIndex indexOf(const QString &id) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;

    // Drops of groups among groups, projects among the projects of their
    // group, and tasks among the tasks of their project, as in the M3 plan,
    // part 6. A drop changes nothing itself: it emits moveRequested.
    Qt::DropActions supportedDropActions() const override;
    bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                         const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;

signals:
    // A dropped group, project or task, and its new place among its siblings,
    // hidden ones included. A drop never changes a priority.
    void moveRequested(mirante::SidebarModel::Kind kind, const QString &id, int index);

private:
    void emitChanged(const QModelIndex &parent);

    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
