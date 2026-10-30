#include "ui/SidebarModel.h"

#include "api/Attention.h"
#include "core/GroupColours.h"
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "layout/DragMime.h"

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QHash>
#include <QMimeData>
#include <QPalette>

#include <algorithm>
#include <cstddef>
#include <optional>
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
        // The projects to show, each with its tasks. The Trash list shows a
        // project in the trash with all its tasks, and any other project only
        // with its tasks in the trash.
        std::vector<std::pair<const Project *, std::vector<const Task *>>> projects;
        for (const Project *project : sortedProjects(model.projectsOfGroup(group->id), filter)) {
            std::vector<const Task *> tasks = filter.trash
                ? trashContents(*project, model.tasksOfProject(project->id))
                : sortedTasks(model.tasksOfProject(project->id), filter);
            if (tasks.empty() && (filter.trash ? !project->isTrashed() : filter.hideEmptyProjects))
                continue;
            projects.emplace_back(project, std::move(tasks));
        }
        // The Trash list leaves out groups with nothing in the trash.
        if (filter.trash && projects.empty())
            continue;
        const int groupNode = addNode(tree, SidebarModel::Kind::Group, group->id, {}, -1);
        for (const auto &[project, tasks] : projects) {
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

// A sidebar reorder drag. It has a type of its own, so dashboards, which
// accept only DragMime payloads, never take it. The data is the id of the
// group, project or task, or for an item or a task dashboard, the task id and
// its id with a line break between them, since those ids are unique only
// within their task.
constexpr char moveMimeType[] = "application/x-mirante-sidebar-move";

// What a reorder drop asks for.
struct Move
{
    SidebarModel::Kind kind = SidebarModel::Kind::Group;
    QString id;
    int index = 0;
    QString taskId;  // for an item or a dashboard
};

// The position of the entry with id in list, or -1.
template <typename T>
int positionOf(const std::vector<const T *> &list, const QString &id)
{
    const auto it = std::find_if(list.begin(), list.end(), [&id](const T *e) { return e->id == id; });
    return it == list.end() ? -1 : int(it - list.begin());
}

// The index of a project or task dropped between the rows above and below,
// either of which may be absent. The index counts every sibling but the moved
// one, hidden ones included, as edits::moveProject and moveTask do. The
// priority does not change.
template <typename T, typename Before>
void placeAmong(Move &move, const std::vector<const T *> &siblings, const T *moved, const T *above, const T *below,
                Before before)
{
    std::vector<const T *> others;
    for (const T *sibling : siblings) {
        if (sibling->id != moved->id)
            others.push_back(sibling);
    }
    std::sort(others.begin(), others.end(), [&before](const T *a, const T *b) { return before(*a, *b); });
    if (above)
        move.index = positionOf(others, above->id) + 1;
    else if (below)
        move.index = positionOf(others, below->id);
    else
        move.index = 0;
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

// A tooltip's line for the attention badge, with its line break, or nothing.
QString attentionLine(int count)
{
    if (count <= 0)
        return {};
    return QLatin1Char('\n') + SidebarModel::tr("Items that need attention: %1").arg(count);
}

} // namespace

struct SidebarModel::Private
{
    const Model &model;
    const ItemTypeRegistry &types;
    SortFilter filter;
    Tree tree;
    const Attention *attention = nullptr;
    QMetaObject::Connection attentionConnection;
    // The attention counts last reported, by group, project and task id, so
    // countsChanged repaints only the rows whose count changed.
    QHash<QString, int> counts;

    Private(const Model &m, const ItemTypeRegistry &t)
        : model(m)
        , types(t)
    {
    }

    int attentionCount(const Node &node) const
    {
        if (!attention)
            return 0;
        switch (node.kind) {
        case Kind::Group: return attention->groupCount(node.id);
        case Kind::Project: return attention->projectCount(node.id);
        case Kind::Task: return attention->taskCount(node.id);
        default: return 0;
        }
    }

    const Node *node(const QModelIndex &index) const
    {
        if (!index.isValid() || index.internalId() >= tree.nodes.size())
            return nullptr;
        return &tree.nodes[std::size_t(index.internalId())];
    }

    const Node *node(const QString &id) const
    {
        const auto it = tree.byId.constFind(id);
        return it == tree.byId.constEnd() ? nullptr : &tree.nodes[std::size_t(*it)];
    }

    // The row an item or dashboard reorder drag names, as moveMimeType data.
    const Node *dragged(const QString &data) const
    {
        const int split = data.indexOf(QLatin1Char('\n'));
        if (split < 0)
            return node(data);
        const QString taskId = data.left(split);
        const QString id = data.mid(split + 1);
        for (const Node &n : tree.nodes) {
            if ((n.kind == Kind::Item || n.kind == Kind::Dashboard) && n.taskId == taskId && n.id == id)
                return &n;
        }
        return nullptr;
    }

    std::optional<Move> move(const QMimeData *data, int row, const QModelIndex &parent) const;
};

// The move a drop of data at row under parent asks for, or nullopt if the
// drop is refused. A drop on a row of the dragged row's kind is a drop above it.
std::optional<Move> SidebarModel::Private::move(const QMimeData *data, int row, const QModelIndex &parent) const
{
    const QString type = QString::fromLatin1(moveMimeType);
    if (filter.trash || !data || !data->hasFormat(type))
        return std::nullopt;
    const Node *moved = dragged(QString::fromUtf8(data->data(type)));
    if (!moved || moved->kind == Kind::ItemList || moved->kind == Kind::DashboardList)
        return std::nullopt;
    const QString id = moved->id;

    if (parent.isValid() && !node(parent))
        return std::nullopt;
    int parentNode = parent.isValid() ? int(parent.internalId()) : -1;
    if (row < 0) {
        if (parentNode < 0)
            return std::nullopt;
        const Node &target = tree.nodes[std::size_t(parentNode)];
        if (target.kind != moved->kind)
            return std::nullopt;
        row = target.row;
        parentNode = target.parent;
    }
    // Only among the siblings it already has.
    if (parentNode != moved->parent)
        return std::nullopt;

    // The visible siblings above and below the drop position.
    const std::vector<int> &children = parentNode < 0 ? tree.roots : tree.nodes[std::size_t(parentNode)].children;
    row = std::clamp(row, 0, int(children.size()));
    std::vector<const Node *> others;
    int position = 0;
    for (int i = 0; i < int(children.size()); ++i) {
        const Node *sibling = &tree.nodes[std::size_t(children[std::size_t(i)])];
        if (sibling == moved)
            continue;
        if (i < row)
            ++position;
        others.push_back(sibling);
    }
    const Node *above = position > 0 ? others[std::size_t(position - 1)] : nullptr;
    const Node *below = position < int(others.size()) ? others[std::size_t(position)] : nullptr;

    Move result;
    result.kind = moved->kind;
    result.id = moved->id;
    if (moved->kind == Kind::Group) {
        result.index = position;
    } else if (moved->kind == Kind::Item || moved->kind == Kind::Dashboard) {
        // Every item and dashboard of the task is a row, so the row is the index.
        result.index = position;
        result.taskId = moved->taskId;
    } else if (moved->kind == Kind::Project) {
        const Project *project = model.project(id);
        if (!project)
            return std::nullopt;
        placeAmong(result, model.projectsOfGroup(project->groupId), project,
                   above ? model.project(above->id) : nullptr, below ? model.project(below->id) : nullptr,
                   projectBefore);
    } else {
        const Task *task = model.task(id);
        if (!task)
            return std::nullopt;
        placeAmong(result, model.tasksOfProject(task->projectId), task, above ? model.task(above->id) : nullptr,
                   below ? model.task(below->id) : nullptr, taskBefore);
    }
    return result;
}

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

    // The same for added and removed tasks, and for groups and projects,
    // whose change repaints only their row.
    const auto rebuild = [this](const QString &id) {
        Tree tree = buildTree(d->model, d->filter);
        if (!tree.sameShape(d->tree)) {
            beginResetModel();
            d->tree = std::move(tree);
            endResetModel();
            return;
        }
        d->tree = std::move(tree);
        const QModelIndex index = indexOf(id);
        if (index.isValid())
            emit dataChanged(index, index);
    };
    for (const auto signal : {&Model::groupAdded, &Model::groupChanged, &Model::groupRemoved, &Model::projectAdded,
                              &Model::projectChanged, &Model::projectRemoved, &Model::taskAdded, &Model::taskRemoved})
        connect(&model, signal, this, rebuild);
    // The project rows of a group are tinted with its colour, so they are
    // repainted too.
    connect(&model, &Model::groupChanged, this, [this](const QString &id) {
        const QModelIndex group = indexOf(id);
        const int rows = rowCount(group);
        if (group.isValid() && rows > 0)
            emit dataChanged(index(0, 0, group), index(rows - 1, 0, group));
    });
}

SidebarModel::~SidebarModel() = default;

void SidebarModel::setFilter(const SortFilter &filter)
{
    if (filter == d->filter)
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
    case AttentionRole:
        if (node->kind != Kind::Group && node->kind != Kind::Project && node->kind != Kind::Task)
            return {};
        return d->attentionCount(*node);
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
        if (role == GroupColourRole)
            return QColor(groupcolours::of(*group));
        if (role == Qt::ToolTipRole) {
            const QString line = attentionLine(d->attentionCount(*node));
            return line.isEmpty() ? QVariant() : QVariant(line.trimmed());
        }
        // Groups are headings: bold and a little larger than the rows under
        // them. SidebarDelegate puts the name in a pill of the group's colour.
        if (role == Qt::FontRole) {
            QFont font;
            font.setBold(true);
            font.setPointSizeF(font.pointSizeF() * 1.08);
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
        if (role == GroupColourRole) {
            const Group *group = d->model.group(project->groupId);
            return group ? QVariant(QColor(groupcolours::of(*group))) : QVariant();
        }
        if (role == Qt::ToolTipRole)
            return tr("Priority: %1").arg(priorityName(project->priority)) + attentionLine(d->attentionCount(*node));
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
            return tr("Priority: %1").arg(priorityName(task->priority)) + attentionLine(d->attentionCount(*node));
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
    // The top level, groups, projects and the two lists of a task take drops
    // among their children.
    // Nothing in the Trash list is dragged or takes a drop.
    if (!index.isValid())
        return d->filter.trash ? Qt::NoItemFlags : Qt::ItemIsDropEnabled;
    const Node *node = d->node(index);
    if (!node)
        return Qt::NoItemFlags;
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (d->filter.trash)
        return node->kind == Kind::Item || node->kind == Kind::Dashboard ? flags | Qt::ItemNeverHasChildren
                                                                         : flags;
    switch (node->kind) {
    case Kind::Item:
    case Kind::Dashboard:
        flags |= Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
        break;
    case Kind::Group:
    case Kind::Project:
        flags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
        break;
    case Kind::Task:
        flags |= Qt::ItemIsDragEnabled;
        break;
    case Kind::ItemList:
    case Kind::DashboardList:
        flags |= Qt::ItemIsDropEnabled;
        break;
    }
    return flags;
}

QStringList SidebarModel::mimeTypes() const
{
    return {QString::fromLatin1(dragMimeType), QString::fromLatin1(moveMimeType)};
}

Qt::DropActions SidebarModel::supportedDropActions() const { return Qt::CopyAction | Qt::MoveAction; }

bool SidebarModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*column*/,
                                   const QModelIndex &parent) const
{
    if (action != Qt::CopyAction && action != Qt::MoveAction)
        return false;
    return d->move(data, row, parent).has_value();
}

bool SidebarModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int /*column*/,
                                const QModelIndex &parent)
{
    if (action != Qt::CopyAction && action != Qt::MoveAction)
        return false;
    const std::optional<Move> move = d->move(data, row, parent);
    if (!move)
        return false;
    emit moveRequested(move->kind, move->id, move->index, move->taskId);
    return true;
}

// A drag carries one entry: the first draggable one among the indexes. An
// item or a dashboard is a DragMime reference, which dashboards accept; a
// dashboard is dropped as a task cell showing that dashboard. It also carries
// the reorder type, to move it among its siblings. A group, project or task
// only reorders the sidebar, under a type dashboards do not accept.
QMimeData *SidebarModel::mimeData(const QModelIndexList &indexes) const
{
    for (const QModelIndex &index : indexes) {
        const Node *node = d->node(index);
        if (!node)
            continue;
        if (node->kind == Kind::Group || node->kind == Kind::Project || node->kind == Kind::Task) {
            auto *data = new QMimeData;
            data->setData(QString::fromLatin1(moveMimeType), node->id.toUtf8());
            return data;
        }
        if (node->kind != Kind::Item && node->kind != Kind::Dashboard)
            continue;
        DragPayload payload;
        payload.ref.task = node->taskId;
        if (node->kind == Kind::Item)
            payload.ref.item = node->id;
        else
            payload.ref.dashboard = node->id;
        QMimeData *data = dragMimeData(payload);
        data->setData(QString::fromLatin1(moveMimeType), (node->taskId + QLatin1Char('\n') + node->id).toUtf8());
        return data;
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


void SidebarModel::setAttention(const Attention *attention)
{
    if (attention == d->attention)
        return;
    disconnect(d->attentionConnection);
    d->attention = attention;

    // Repaints the group, project and task rows whose count changed.
    const auto update = [this] {
        for (const Node &node : d->tree.nodes) {
            if (node.kind != Kind::Group && node.kind != Kind::Project && node.kind != Kind::Task)
                continue;
            const int count = d->attentionCount(node);
            if (count == d->counts.value(node.id))
                continue;
            if (count == 0)
                d->counts.remove(node.id);
            else
                d->counts.insert(node.id, count);
            const QModelIndex index = indexOf(node.id);
            emit dataChanged(index, index, {AttentionRole, Qt::ToolTipRole});
        }
    };
    if (attention)
        d->attentionConnection = connect(attention, &Attention::countsChanged, this, update);
    update();
}

} // namespace mirante
