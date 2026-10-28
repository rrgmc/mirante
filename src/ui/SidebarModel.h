#pragma once

#include "core/Sorting.h"

#include <QAbstractItemModel>

#include <memory>

namespace mirante {

class ItemTypeRegistry;
class Model;

// The sidebar tree over Model: groups, then projects, then tasks, then items.
// Groups use their manual order; projects and tasks are sorted with Sorting.
// Tasks and items can be dragged, as DragMime payloads.
// Implemented in SidebarModel.cpp.
class SidebarModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Kind { Group, Project, Task, Item };

    enum Role {
        KindRole = Qt::UserRole + 1,    // Kind, as int
        IdRole,                         // the entity id
        TaskIdRole,                     // for tasks and items: the task id
        PriorityRole,                   // for projects and tasks: Priority, as int
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

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
