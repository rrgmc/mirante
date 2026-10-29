#include "ui/SidebarModel.h"

#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "layout/DragMime.h"

#include <QFont>
#include <QIcon>
#include <QHash>
#include <QMimeData>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace mirante {

namespace {

// One row of the tree. Rows hold ids, not pointers, because the model may
// replace an entity while the row lives on.
struct Node
{
    SidebarModel::Kind kind = SidebarModel::Kind::Group;
    QString id;
    QString taskId;             // for a task and everything under it
    int parent = -1;            // node index, or -1 for a group
    int row = 0;
    std::vector<int> children;  // node indices
};

struct Tree
{
    std::vector<Node> nodes;
    std::vector<int> roots;
    QHash<QString, int> byId;

    // True if both trees have the same rows in the same places.
    bool sameShape(const Tree &other) const
    {
        if (nodes.size() != other.nodes.size() || roots != other.roots)
            return false;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const Node &a = nodes[i];
            const Node &b = other.nodes[i];
            if (a.kind != b.kind || a.id != b.id || a.parent != b.parent)
                return false;
        }
        return true;
    }
};

bool groupBefore(const Group *a, const Group *b)
{
    if (a->order != b->order)
        return a->order < b->order;
    if (const int c = QString::compare(a->name, b->name, Qt::CaseInsensitive); c != 0)
        return c < 0;
    return a->id < b->id;
}

int addNode(Tree &tree, SidebarModel::Kind kind, const QString &id, const QString &taskId, int parent)
{
    const int index = int(tree.nodes.size());
    Node node;
    node.kind = kind;
    node.id = id;
    node.taskId = taskId;
    node.parent = parent;
    std::vector<int> &siblings = parent < 0 ? tree.roots : tree.nodes[std::size_t(parent)].children;
    node.row = int(siblings.size());
    siblings.push_back(index);
    tree.nodes.push_back(std::move(node));
    if (!tree.byId.contains(id))
        tree.byId.insert(id, index);
    return index;
}

Tree buildTree(const Model &model, const SortFilter &filter)
{
    Tree tree;
    std::vector<const Group *> groups = model.groups();
    std::sort(groups.begin(), groups.end(), groupBefore);
    for (const Group *group : groups) {
        const int groupNode = addNode(tree, SidebarModel::Kind::Group, group->id, {}, -1);
        for (const Project *project : sortedProjects(model.projectsOfGroup(group->id), filter)) {
            const std::vector<const Task *> tasks = sortedTasks(model.tasksOfProject(project->id), filter);
            if (filter.hideEmptyProjects && tasks.empty())
                continue;
            const int projectNode = addNode(tree, SidebarModel::Kind::Project, project->id, {}, groupNode);
            for (const Task *task : tasks) {
                const int taskNode = addNode(tree, SidebarModel::Kind::Task, task->id, task->id, projectNode);
                // The two lists have ids of their own, so their expansion is remembered.
                const int dashboards = addNode(tree, SidebarModel::Kind::DashboardList,
                                               task->id + QStringLiteral("/dashboards"), task->id, taskNode);
                for (const Dashboard &dashboard : task->dashboards)
                    addNode(tree, SidebarModel::Kind::Dashboard, dashboard.id, task->id, dashboards);
                const int items = addNode(tree, SidebarModel::Kind::ItemList, task->id + QStringLiteral("/items"),
                                          task->id, taskNode);
                for (const Item &item : task->items)
                    addNode(tree, SidebarModel::Kind::Item, item.id, task->id, items);
            }
        }
    }
    return tree;
}

QString priorityName(Priority priority)
{
    switch (priority) {
    case Priority::Urgent: return SidebarModel::tr("Urgent");
    case Priority::High: return SidebarModel::tr("High");
    case Priority::Normal: return SidebarModel::tr("Normal");
    case Priority::Low: return SidebarModel::tr("Low");
    }
    return {};
}

} // namespace

struct SidebarModel::Private
{
    const Model &model;
    const ItemTypeRegistry &types;
    SortFilter filter;
    Tree tree;

    Private(const Model &m, const ItemTypeRegistry &t)
        : model(m)
        , types(t)
    {
    }

    const Node *node(const QModelIndex &index) const
    {
        if (!index.isValid() || index.internalId() >= tree.nodes.size())
            return nullptr;
        return &tree.nodes[std::size_t(index.internalId())];
    }
};

SidebarModel::SidebarModel(const Model &model, const ItemTypeRegistry &types, QObject *parent)
    : QAbstractItemModel(parent)
    , d(std::make_unique<Private>(model, types))
{
    d->tree = buildTree(d->model, d->filter);

    connect(&model, &Model::modelReset, this, [this] {
        beginResetModel();
        d->tree = buildTree(d->model, d->filter);
        endResetModel();
    });

    // A change that moves, hides or adds rows resets the tree. Otherwise the
    // task and its items are only repainted.
    connect(&model, &Model::taskChanged, this, [this](const QString &taskId) {
        Tree tree = buildTree(d->model, d->filter);
        if (!tree.sameShape(d->tree)) {
            beginResetModel();
            d->tree = std::move(tree);
            endResetModel();
            return;
        }
        d->tree = std::move(tree);
        const QModelIndex task = indexOf(taskId);
        if (!task.isValid())
            return;
        emitChanged(task);
    });
}

SidebarModel::~SidebarModel() = default;

void SidebarModel::setFilter(const SortFilter &filter)
{
    if (filter.hideLow == d->filter.hideLow && filter.closedTasks == d->filter.closedTasks
        && filter.hideEmptyProjects == d->filter.hideEmptyProjects)
        return;
    beginResetModel();
    d->filter = filter;
    d->tree = buildTree(d->model, d->filter);
    endResetModel();
}

SortFilter SidebarModel::filter() const { return d->filter; }

QModelIndex SidebarModel::indexOf(const QString &id) const
{
    const auto it = d->tree.byId.constFind(id);
    if (it == d->tree.byId.constEnd())
        return {};
    return createIndex(d->tree.nodes[std::size_t(*it)].row, 0, quintptr(*it));
}

QModelIndex SidebarModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0 || (parent.isValid() && parent.column() != 0))
        return {};
    const Node *parentNode = d->node(parent);
    if (parent.isValid() && !parentNode)
        return {};
    const std::vector<int> &children = parentNode ? parentNode->children : d->tree.roots;
    if (row >= int(children.size()))
        return {};
    return createIndex(row, 0, quintptr(children[std::size_t(row)]));
}

QModelIndex SidebarModel::parent(const QModelIndex &child) const
{
    const Node *node = d->node(child);
    if (!node || node->parent < 0)
        return {};
    const Node &parentNode = d->tree.nodes[std::size_t(node->parent)];
    return createIndex(parentNode.row, 0, quintptr(node->parent));
}

int SidebarModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return int(d->tree.roots.size());
    if (parent.column() != 0)
        return 0;
    const Node *node = d->node(parent);
    return node ? int(node->children.size()) : 0;
}

int SidebarModel::columnCount(const QModelIndex & /*parent*/) const { return 1; }

QVariant SidebarModel::data(const QModelIndex &index, int role) const
{
    const Node *node = d->node(index);
    if (!node)
        return {};

    switch (role) {
    case KindRole:
        return int(node->kind);
    case IdRole:
        return node->id;
    case TaskIdRole:
        return node->taskId.isEmpty() ? QVariant() : QVariant(node->taskId);
    default:
        break;
    }

    switch (node->kind) {
    case Kind::Group: {
        const Group *group = d->model.group(node->id);
        if (!group)
            return {};
        if (role == Qt::DisplayRole)
            return group->name;
        if (role == Qt::FontRole) {
            QFont font;
            font.setBold(true);
            return font;
        }
        return {};
    }
    case Kind::Project: {
        const Project *project = d->model.project(node->id);
        if (!project)
            return {};
        if (role == Qt::DisplayRole)
            return project->name;
        if (role == PriorityRole)
            return int(project->priority);
        if (role == Qt::ToolTipRole)
            return tr("Priority: %1").arg(priorityName(project->priority));
        return {};
    }
    case Kind::Task: {
        const Task *task = d->model.task(node->id);
        if (!task)
            return {};
        if (role == Qt::DisplayRole)
            return task->title;
        if (role == PriorityRole)
            return int(task->priority);
        if (role == Qt::ToolTipRole)
            return tr("Priority: %1").arg(priorityName(task->priority));
        if (role == Qt::FontRole && task->state != TaskState::Active) {
            QFont font;
            font.setStrikeOut(task->state == TaskState::Done);
            font.setItalic(task->state == TaskState::Archived);
            return font;
        }
        return {};
    }
    case Kind::ItemList:
        if (role == Qt::DisplayRole)
            return tr("Items");
        if (role == Qt::ToolTipRole)
            return tr("Drag an item onto a dashboard to show it there");
        return {};
    case Kind::DashboardList:
        if (role == Qt::DisplayRole)
            return tr("Dashboards");
        if (role == Qt::ToolTipRole)
            return tr("Drag a dashboard onto a root dashboard to show the task there");
        return {};
    case Kind::Dashboard: {
        const Dashboard *dashboard = d->model.taskDashboard(node->taskId, node->id);
        if (!dashboard)
            return {};
        if (role == Qt::DisplayRole)
            return dashboard->name;
        if (role == Qt::DecorationRole)
            return QIcon(QStringLiteral(":/icons/dashboard.svg"));
        if (role == Qt::ToolTipRole) {
            const Task *task = d->model.task(node->taskId);
            return task && task->defaultDashboardId == dashboard->id ? tr("Task dashboard, opened by default")
                                                                     : tr("Task dashboard");
        }
        return {};
    }
    case Kind::Item: {
        const Item *item = d->model.item(node->taskId, node->id);
        if (!item)
            return {};
        const ItemType &type = d->types.type(item->type);
        if (role == Qt::DisplayRole)
            return item->name.isEmpty() ? type.name() : item->name;
        if (role == Qt::DecorationRole)
            return type.icon();
        if (role == Qt::ToolTipRole)
            return type.name();
        return {};
    }
    }
    return {};
}

Qt::ItemFlags SidebarModel::flags(const QModelIndex &index) const
{
    const Node *node = d->node(index);
    if (!node)
        return Qt::NoItemFlags;
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (node->kind == Kind::Item || node->kind == Kind::Dashboard)
        flags |= Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
    return flags;
}

QStringList SidebarModel::mimeTypes() const { return {QString::fromLatin1(dragMimeType)}; }

// Stubs written by T0 of the M3 plan. T10 implements them.
Qt::DropActions SidebarModel::supportedDropActions() const { return Qt::IgnoreAction; }

bool SidebarModel::canDropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex &) const
{
    return false;
}

bool SidebarModel::dropMimeData(const QMimeData *, Qt::DropAction, int, int, const QModelIndex &) { return false; }

// A drag carries one reference: the first item or dashboard among the indexes.
// A dashboard is dropped as a task cell showing that dashboard.
QMimeData *SidebarModel::mimeData(const QModelIndexList &indexes) const
{
    for (const QModelIndex &index : indexes) {
        const Node *node = d->node(index);
        if (!node || (node->kind != Kind::Item && node->kind != Kind::Dashboard))
            continue;
        DragPayload payload;
        payload.ref.task = node->taskId;
        if (node->kind == Kind::Item)
            payload.ref.item = node->id;
        else
            payload.ref.dashboard = node->id;
        return dragMimeData(payload);
    }
    return nullptr;
}

// Repaints a row and every row under it.
void SidebarModel::emitChanged(const QModelIndex &parent)
{
    emit dataChanged(parent, parent);
    const int rows = rowCount(parent);
    for (int row = 0; row < rows; ++row)
        emitChanged(index(row, 0, parent));
}

} // namespace mirante
