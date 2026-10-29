#include "layout/LayoutEditor.h"

#include "core/Sorting.h"
#include "items/ItemTypeRegistry.h"
#include "layout/LayoutServices.h"
#include "layout/LayoutView.h"
#include "layout/TaskCell.h"

#include <mirante/ItemType.h>

#include <QAction>
#include <QDrag>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QWebEngineView>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace mirante {

namespace {

using Kind = LayoutNode::Kind;

constexpr int dragPixmapWidth = 240;

bool isEmptyRef(const CellRef &ref) { return ref.item.isEmpty() && ref.task.isEmpty() && ref.dashboard.isEmpty(); }

NodePath childOf(NodePath path, int index)
{
    path.push_back(index);
    return path;
}

std::optional<NodePath> parentOf(const NodePath &path)
{
    if (path.empty())
        return std::nullopt;
    return NodePath(path.begin(), path.end() - 1);
}

// True if the node at path is a tab of a tab group.
bool isTab(const LayoutNode &tree, const NodePath &path)
{
    const std::optional<NodePath> parent = parentOf(path);
    const LayoutNode *node = parent ? layout::nodeAt(tree, *parent) : nullptr;
    return node && node->kind == Kind::Tabs;
}

// The commands of a menu, run after the menu has closed.
using Commands = std::map<QAction *, std::function<void()>>;

QAction *addCommand(QMenu *menu, Commands &commands, const QString &text, std::function<void()> command)
{
    QAction *action = menu->addAction(text);
    commands[action] = std::move(command);
    return action;
}

} // namespace

struct LayoutEditor::Private
{
    LayoutEditor *q = nullptr;
    Model &model;
    const ItemTypeRegistry &types;
    std::map<LayoutView *, DashboardRef> views;
    std::map<LayoutView *, QPointer<DropOverlay>> overlays;
    const LayoutServices *services = nullptr;

    std::function<bool(QWidget *, const QString &)> confirm = [](QWidget *parent, const QString &question) {
        return QMessageBox::question(parent, LayoutEditor::tr("Remove"), question) == QMessageBox::Yes;
    };

    // A drop on a cell dragged from a view is applied after the drag ends,
    // because the drag runs inside the event handler of the dragged cell.
    int dragging = 0;
    std::optional<std::function<void()>> pendingDrop;

    Private(Model &m, const ItemTypeRegistry &t)
        : model(m)
        , types(t)
    {
    }

    // Marks the cells whose task, dashboard or item is gone as removed, as
    // normalisation would, but keeps the tree's shape, so drop paths stay
    // valid. A stored layout keeps such references until it next changes, and
    // a drop on a removed cell must fill it like an empty one.
    void markRemoved(const DashboardRef &dashboard, LayoutNode &node) const
    {
        if (node.kind == Kind::Cell) {
            if (node.ref && !node.removed && !refShown(dashboard, *node.ref)) {
                node.ref.reset();
                node.removed = true;
            }
            return;
        }
        for (LayoutNode &child : node.children)
            markRemoved(dashboard, child);
    }

    // Whether a cell of the dashboard can show ref: in a task dashboard, an
    // item of that task only, as Model::setTaskDashboardLayout requires.
    bool refShown(const DashboardRef &dashboard, const CellRef &ref) const
    {
        if (!dashboard.isRoot() && ref.isItem())
            return (ref.task.isEmpty() || ref.task == dashboard.taskId) && model.item(dashboard.taskId, ref.item);
        return model.refExists(ref);
    }

    // References
    bool accepts(const DashboardRef &dashboard, const CellRef &ref, std::optional<CellRef> *stored) const;
    QString titleFor(const DashboardRef &dashboard, const LayoutNode *node) const;
    LayoutNode nameUntitledTabs(const DashboardRef &dashboard, const LayoutNode &tree, const NodePath &path) const;
    std::optional<EditResult> insert(const DashboardRef &dashboard, const LayoutNode &tree, const DropTarget &target,
                                     const std::optional<CellRef> &ref, const QString &title) const;

    // Drops. A drop is planned first and stored only if the plan exists, so
    // the overlay can ask whether a drop would work without making it.
    struct DropPlan
    {
        std::optional<DashboardRef> source;     // set when a cell leaves another dashboard
        LayoutNode sourceTree;
        DashboardRef target;
        LayoutNode tree;
    };
    std::optional<DropPlan> planDrop(const DashboardRef &dashboard, const DropTarget &target,
                                     const DragPayload &payload) const;
    std::optional<std::pair<DashboardRef, DropTarget>> resolveDrop(LayoutView *view, const DropTarget &target,
                                                                   const DragPayload &payload) const;

    // Views
    void forget(LayoutView *view);
    void execMenu(QMenu &menu, const Commands &commands, const QPoint &globalPos);
    void showCellMenu(LayoutView *view, const NodePath &path, const QPoint &globalPos);
    std::optional<std::pair<QString, QString>> itemOfCell(const DashboardRef &dashboard,
                                                          const LayoutNode &node) const;
    void setItemField(const QString &taskId, const QString &itemId, const QString &key, const QString &value);
    void addViewCommands(QMenu &menu, Commands &commands, LayoutView *view, const DashboardRef &dashboard,
                         const NodePath &path, const LayoutNode &node) const;
    void addItemCommands(QMenu &menu, Commands &commands, const QWidget *cell, const QString &taskId,
                         const QString &itemId);
    void showTabMenu(LayoutView *view, const NodePath &tabsPath, int index, const QPoint &globalPos);
    void startDrag(LayoutView *view, const NodePath &path);
    void onDropped(LayoutView *view, const DropTarget &target, const DragPayload &payload);
    void dropFromView(LayoutView *view, const DropTarget &target, const DragPayload &payload);
};

// Says whether the dashboard can hold ref, and sets *stored to the reference
// as that dashboard stores it. An empty reference stands for an empty cell.
bool LayoutEditor::Private::accepts(const DashboardRef &dashboard, const CellRef &ref,
                                    std::optional<CellRef> *stored) const
{
    if (isEmptyRef(ref)) {
        *stored = std::nullopt;
        return true;
    }

    // A task dashboard holds only items of its own task.
    if (!dashboard.isRoot()) {
        if (!ref.isItem() || (!ref.task.isEmpty() && ref.task != dashboard.taskId)
            || !model.item(dashboard.taskId, ref.item))
            return false;
        *stored = CellRef{ref.item, QString(), QString()};
        return true;
    }

    if (ref.isItem()) {
        QString taskId = ref.task;
        if (taskId.isEmpty()) {
            const Task *task = model.taskOfItem(ref.item);
            if (!task)
                return false;
            taskId = task->id;
        }
        if (!model.item(taskId, ref.item))
            return false;
        *stored = CellRef{ref.item, taskId, QString()};
        return true;
    }

    if (ref.isTask()) {
        const Task *task = model.task(ref.task);
        if (!task)
            return false;
        QString dashboardId = ref.dashboard;
        if (dashboardId.isEmpty() || !model.taskDashboard(task->id, dashboardId))
            dashboardId = task->defaultDashboardId;
        *stored = CellRef{QString(), task->id, dashboardId};
        return true;
    }
    return false;
}

// The title of a tab made around a node: the item name, the task title or "Tab".
QString LayoutEditor::Private::titleFor(const DashboardRef &dashboard, const LayoutNode *node) const
{
    if (node && node->kind == Kind::Cell && node->ref && !node->removed) {
        const CellRef &ref = *node->ref;
        if (ref.isItem()) {
            QString taskId = dashboard.isRoot() ? ref.task : dashboard.taskId;
            if (taskId.isEmpty()) {
                if (const Task *task = model.taskOfItem(ref.item))
                    taskId = task->id;
            }
            if (const Item *item = model.item(taskId, ref.item); item && !item->name.isEmpty())
                return item->name;
        } else if (ref.isTask()) {
            if (const Task *task = model.task(ref.task); task && !task->title.isEmpty())
                return task->title;
        }
    }
    return LayoutEditor::tr("Tab");
}

// Gives the untitled tabs of the tab group holding the node at path a title,
// such as the tab of a node that addTab or moveNode wrapped in a new group.
LayoutNode LayoutEditor::Private::nameUntitledTabs(const DashboardRef &dashboard, const LayoutNode &tree,
                                                   const NodePath &path) const
{
    const std::optional<NodePath> tabsPath = parentOf(path);
    const LayoutNode *tabs = tabsPath ? layout::nodeAt(tree, *tabsPath) : nullptr;
    if (!tabs || tabs->kind != Kind::Tabs)
        return tree;
    LayoutNode result = tree;
    for (std::size_t i = 0; i < tabs->children.size(); ++i) {
        if (i >= tabs->titles.size() || tabs->titles[i].isEmpty())
            result = layout::renameTab(result, *tabsPath, int(i), titleFor(dashboard, &tabs->children[i]));
    }
    return result;
}

// Puts a new cell with ref in the zone of the target node.
std::optional<EditResult> LayoutEditor::Private::insert(const DashboardRef &dashboard, const LayoutNode &tree,
                                                        const DropTarget &target, const std::optional<CellRef> &ref,
                                                        const QString &title) const
{
    const LayoutNode *node = layout::nodeAt(tree, target.path);
    if (!node)
        return std::nullopt;

    switch (target.zone.kind) {
    case DropZone::Kind::Edge:
        return layout::splitCell(tree, target.path, target.zone.edge, ref);

    case DropZone::Kind::Centre:
        if (node->kind == Kind::Cell && (!node->ref || node->removed))
            return EditResult{layout::setRef(tree, target.path, ref), target.path};
        if (node->kind == Kind::Tabs)
            return layout::insertTab(tree, target.path, int(node->children.size()), ref, title);
        {
            EditResult result = layout::addTab(tree, target.path, ref, title);
            result.tree = nameUntitledTabs(dashboard, result.tree, result.path);
            return result;
        }

    case DropZone::Kind::TabBar:
        if (node->kind != Kind::Tabs || target.zone.tabIndex < 0
            || target.zone.tabIndex > int(node->children.size()))
            return std::nullopt;
        return layout::insertTab(tree, target.path, target.zone.tabIndex, ref, title);

    case DropZone::Kind::Replace:
        if (node->kind != Kind::Cell)
            return std::nullopt;
        return EditResult{layout::setRef(tree, target.path, ref), target.path};
    }
    return std::nullopt;
}

void LayoutEditor::Private::forget(LayoutView *view)
{
    views.erase(view);
    overlays.erase(view);
}

// Shows a menu and runs the chosen command. The command may rebuild the view,
// and with it the cell whose event handler is still running, so it runs once
// that handler has returned.
void LayoutEditor::Private::execMenu(QMenu &menu, const Commands &commands, const QPoint &globalPos)
{
    QAction *chosen = menu.exec(globalPos);
    const auto run = commands.find(chosen);
    if (run != commands.end())
        QTimer::singleShot(0, q, run->second);
}

// The task and item that a cell shows, if it shows one that exists.
std::optional<std::pair<QString, QString>> LayoutEditor::Private::itemOfCell(const DashboardRef &dashboard,
                                                                           const LayoutNode &node) const
{
    if (node.kind != Kind::Cell || !node.ref || node.removed || !node.ref->isItem())
        return std::nullopt;
    const QString itemId = node.ref->item;
    QString taskId = dashboard.isRoot() ? node.ref->task : dashboard.taskId;
    if (taskId.isEmpty()) {
        if (const Task *task = model.taskOfItem(itemId))
            taskId = task->id;
    }
    if (!model.item(taskId, itemId))
        return std::nullopt;
    return std::make_pair(taskId, itemId);
}

void LayoutEditor::Private::setItemField(const QString &taskId, const QString &itemId, const QString &key,
                                         const QString &value)
{
    const Task *task = model.task(taskId);
    if (!task)
        return;
    Task changed = *task;
    for (Item &entry : changed.items) {
        if (entry.id == itemId)
            entry.config.insert(key, value);
    }
    model.setTask(changed);
}

// The commands of an item cell that only change how it is shown, so they are
// offered on locked dashboards too: "Maximise" or "Restore", and "Open Alone".
// Item pages, which already show an item alone, are not attached to the
// editor, so they never get here.
void LayoutEditor::Private::addViewCommands(QMenu &menu, Commands &commands, LayoutView *view,
                                            const DashboardRef &dashboard, const NodePath &path,
                                            const LayoutNode &node) const
{
    const auto shown = itemOfCell(dashboard, node);
    if (!services || !shown)
        return;
    const QString taskId = shown->first;
    const QString itemId = shown->second;

    if (const auto toggle = services->toggleMaximised) {
        const QPointer<QWidget> cell = view->cellAt(path);
        const bool maximised = cell && services->isMaximised && services->isMaximised(cell);
        addCommand(&menu, commands, maximised ? LayoutEditor::tr("Restore") : LayoutEditor::tr("Maximise"),
                   [toggle, cell] {
                       if (cell)
                           toggle(cell);
                   });
    }
    if (const auto openAlone = services->openItemAlone) {
        addCommand(&menu, commands, LayoutEditor::tr("Open Alone"),
                   [openAlone, taskId, itemId] { openAlone(taskId, itemId); });
    }
}

// The commands that change the item, not the layout, so they are offered on
// locked dashboards and item pages too: the item dialog, and, for an item
// whose type has an address field and whose cell shows a web page, a command
// that saves the page's current address in that field.
void LayoutEditor::Private::addItemCommands(QMenu &menu, Commands &commands, const QWidget *cell,
                                            const QString &taskId, const QString &itemId)
{
    if (!services)
        return;
    if (const auto edit = services->editItem)
        addCommand(&menu, commands, LayoutEditor::tr("Edit Item…"), [edit, taskId, itemId] { edit(taskId, itemId); });

    // The first address field of the item's type.
    const Item *item = model.item(taskId, itemId);
    QString key;
    if (item && services->types) {
        for (const Field &field : services->types->type(item->type).fields()) {
            if (field.type == Field::Type::Url) {
                key = field.key;
                break;
            }
        }
    }
    const auto *page = cell ? cell->findChild<QWebEngineView *>() : nullptr;
    const QUrl current = page ? page->url() : QUrl();
    if (item && !key.isEmpty()
        && (current.scheme() == QStringLiteral("http") || current.scheme() == QStringLiteral("https"))) {
        const QString address = current.toString();
        QAction *use = addCommand(&menu, commands, LayoutEditor::tr("Use Current Address"),
                                  [this, taskId, itemId, key, address] { setItemField(taskId, itemId, key, address); });
        use->setToolTip(address);
        use->setEnabled(item->config.value(key).toString() != address);
    }
}

void LayoutEditor::Private::showCellMenu(LayoutView *view, const NodePath &path, const QPoint &globalPos)
{
    const auto found = views.find(view);
    if (found == views.end())
        return;
    const DashboardRef dashboard = found->second;
    const LayoutNode *tree = model.layout(dashboard);
    const LayoutNode *node = tree ? layout::nodeAt(*tree, path) : nullptr;
    if (!node)
        return;

    QMenu menu(view);
    Commands commands;
    addViewCommands(menu, commands, view, dashboard, path, *node);
    if (const auto shown = itemOfCell(dashboard, *node)) {
        if (!commands.empty())
            menu.addSeparator();
        addItemCommands(menu, commands, view->cellAt(path), shown->first, shown->second);
    }
    // A locked dashboard offers only the commands that do not edit its layout.
    if (!view->editMode()) {
        if (!commands.empty())
            execMenu(menu, commands, globalPos);
        return;
    }
    if (!commands.empty())
        menu.addSeparator();
    const auto command = [&](const QString &text, CellCommand cellCommand) {
        return addCommand(&menu, commands, text,
                          [this, dashboard, path, cellCommand] { q->cellCommand(dashboard, path, cellCommand); });
    };
    // A task dashboard is edited on its own, not inside the root dashboard.
    if (QPointer<TaskCell> taskCell = qobject_cast<TaskCell *>(view->cellAt(path))) {
        addCommand(&menu, commands, LayoutEditor::tr("Edit task dashboard"), [taskCell] {
            if (taskCell)
                taskCell->openAlone();
        });
        menu.addSeparator();
    }
    command(LayoutEditor::tr("Split right"), CellCommand::SplitRight);
    command(LayoutEditor::tr("Split down"), CellCommand::SplitDown);
    // A cell that is already a tab gets more tabs with "Add tab". Wrapping it
    // again would only nest another tab bar.
    if (!isTab(*tree, path))
        command(LayoutEditor::tr("Wrap in tabs"), CellCommand::WrapInTabs);
    command(LayoutEditor::tr("Add tab"), CellCommand::AddTab);
    menu.addSeparator();
    // These two remove content, so they ask first.
    QPointer<LayoutView> guard(view);
    const auto confirmed = [&](const QString &text, CellCommand cellCommand, const QString &question) {
        return addCommand(&menu, commands, text, [this, guard, dashboard, path, cellCommand, question] {
            if (guard && confirm(guard, question))
                q->cellCommand(dashboard, path, cellCommand);
        });
    };
    confirmed(LayoutEditor::tr("Clear cell"), CellCommand::ClearCell,
              LayoutEditor::tr("Clear this cell? It stays in the layout, empty."))
        ->setEnabled(node->kind == Kind::Cell && node->ref.has_value());
    confirmed(LayoutEditor::tr("Remove cell"), CellCommand::RemoveCell,
              LayoutEditor::tr("Remove this cell from the dashboard?"))
        ->setEnabled(!path.empty());

    execMenu(menu, commands, globalPos);
}

void LayoutEditor::Private::showTabMenu(LayoutView *view, const NodePath &tabsPath, int index,
                                        const QPoint &globalPos)
{
    const auto found = views.find(view);
    if (found == views.end() || !view->editMode() || index < 0)
        return;
    const DashboardRef dashboard = found->second;
    const LayoutNode *tree = model.layout(dashboard);
    const LayoutNode *tabs = tree ? layout::nodeAt(*tree, tabsPath) : nullptr;
    if (!tabs || tabs->kind != Kind::Tabs || index >= int(tabs->children.size()))
        return;
    const QString title = std::size_t(index) < tabs->titles.size() ? tabs->titles[std::size_t(index)] : QString();

    QMenu menu(view);
    Commands commands;
    const auto command = [&](const QString &text, TabCommand tabCommand) {
        return addCommand(&menu, commands, text, [this, dashboard, tabsPath, index, tabCommand] {
            q->tabCommand(dashboard, tabsPath, index, tabCommand);
        });
    };
    command(LayoutEditor::tr("Add tab"), TabCommand::AddTab);
    menu.addSeparator();
    QPointer<LayoutView> guard(view);
    addCommand(&menu, commands, LayoutEditor::tr("Rename…"), [this, guard, dashboard, tabsPath, index, title] {
        bool ok = false;
        const QString renamed = QInputDialog::getText(guard, LayoutEditor::tr("Rename tab"),
                                                      LayoutEditor::tr("Title:"), QLineEdit::Normal, title, &ok)
                                    .trimmed();
        if (ok && !renamed.isEmpty())
            q->renameTab(dashboard, tabsPath, index, renamed);
    });
    addCommand(&menu, commands, LayoutEditor::tr("Delete tab"), [this, guard, dashboard, tabsPath, index, title] {
        const QString question = LayoutEditor::tr("Delete the tab \"%1\" and remove its content from the dashboard?")
                                     .arg(title.isEmpty() ? LayoutEditor::tr("untitled") : title);
        if (guard && confirm(guard, question))
            q->tabCommand(dashboard, tabsPath, index, TabCommand::Delete);
    });
    command(LayoutEditor::tr("Move left"), TabCommand::MoveLeft)->setEnabled(index > 0);
    command(LayoutEditor::tr("Move right"), TabCommand::MoveRight)->setEnabled(index + 1 < int(tabs->children.size()));
    // Only a group of one tab can be ungrouped: its tab's content takes its place.
    if (tabs->children.size() == 1) {
        menu.addSeparator();
        command(LayoutEditor::tr("Ungroup"), TabCommand::Ungroup);
    }

    execMenu(menu, commands, globalPos);
}

void LayoutEditor::Private::startDrag(LayoutView *view, const NodePath &path)
{
    const auto found = views.find(view);
    const LayoutNode *node = layout::nodeAt(view->tree(), path);
    if (found == views.end() || !node)
        return;

    // A cell carries its reference. A tab group or a split carries only where
    // it comes from, so it moves within its own dashboard.
    DragPayload payload;
    payload.ref = node->kind == Kind::Cell ? node->ref.value_or(CellRef{}) : CellRef{};
    payload.sourceDashboard = found->second;
    payload.sourcePath = path;

    auto *drag = new QDrag(view);
    drag->setMimeData(dragMimeData(payload));
    if (QWidget *cell = view->widgetAt(path); cell && cell->width() > 0) {
        QPixmap pixmap = cell->grab();
        if (pixmap.width() > dragPixmapWidth)
            pixmap = pixmap.scaledToWidth(dragPixmapWidth, Qt::SmoothTransformation);
        drag->setPixmap(pixmap);
        drag->setHotSpot(QPoint(pixmap.width() / 2, 0));
    }

    ++dragging;
    drag->exec(Qt::MoveAction);     // Qt deletes the drag
    --dragging;
    if (dragging == 0 && pendingDrop) {
        const std::function<void()> apply = std::move(*pendingDrop);
        pendingDrop.reset();
        QTimer::singleShot(0, q, apply);
    }
}

void LayoutEditor::Private::onDropped(LayoutView *view, const DropTarget &target, const DragPayload &payload)
{
    const QPointer<LayoutView> guard(view);
    std::function<void()> apply = [this, guard, target, payload] {
        if (guard)
            dropFromView(guard, target, payload);
    };
    if (dragging > 0)
        pendingDrop = std::move(apply);
    else
        QTimer::singleShot(0, q, apply);
}

void LayoutEditor::Private::dropFromView(LayoutView *view, const DropTarget &target, const DragPayload &payload)
{
    if (const auto resolved = resolveDrop(view, target, payload))
        q->drop(resolved->first, resolved->second, payload);
}

// The dashboard a drop on a view applies to. Tasks belong in root dashboards
// only, so planDrop() refuses a task dropped inside a task cell; it is placed
// beside the task cell by dropping it on the task cell's header instead.
std::optional<std::pair<DashboardRef, DropTarget>>
LayoutEditor::Private::resolveDrop(LayoutView *view, const DropTarget &target, const DragPayload & /*payload*/) const
{
    const auto found = views.find(view);
    if (found == views.end())
        return std::nullopt;
    return std::make_pair(found->second, target);
}

LayoutEditor::LayoutEditor(Model &model, const ItemTypeRegistry &types, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>(model, types))
{
    d->q = this;
}

LayoutEditor::~LayoutEditor()
{
    // The connections read d, so they go before it.
    for (const auto &[view, dashboard] : d->views)
        disconnect(view, nullptr, this, nullptr);
    for (const auto &[view, overlay] : d->overlays)
        delete overlay.data();
}

void LayoutEditor::attach(LayoutView *view, const DashboardRef &dashboard)
{
    if (!view)
        return;
    // Attaching again only changes the dashboard, as when a task cell shows
    // another task dashboard.
    const bool known = d->views.count(view) > 0;
    d->views[view] = dashboard;
    if (known)
        return;

    connect(view, &QObject::destroyed, this, [this, view] { d->forget(view); });
    connect(view, &LayoutView::cellMenuRequested, this,
            [this, view](const NodePath &path, const QPoint &globalPos) { d->showCellMenu(view, path, globalPos); });
    connect(view, &LayoutView::tabMenuRequested, this,
            [this, view](const NodePath &tabsPath, int index, const QPoint &globalPos) {
                d->showTabMenu(view, tabsPath, index, globalPos);
            });
    connect(view, &LayoutView::cellDragStarted, this, [this, view](const NodePath &path) { d->startDrag(view, path); });
    connect(view, &LayoutView::tabAddRequested, this, [this, view](const NodePath &tabsPath) {
        const auto found = d->views.find(view);
        const LayoutNode *tree = found != d->views.end() ? d->model.layout(found->second) : nullptr;
        const LayoutNode *tabs = tree ? layout::nodeAt(*tree, tabsPath) : nullptr;
        if (tabs && tabs->kind == Kind::Tabs && !tabs->children.empty())
            tabCommand(found->second, tabsPath, int(tabs->children.size()) - 1, TabCommand::AddTab);
    });
    connect(view, &LayoutView::treeEdited, this, [this, view](const LayoutNode &tree) {
        const auto found = d->views.find(view);
        if (found != d->views.end())
            d->model.setLayout(found->second, tree);
    });

    auto *overlay = new DropOverlay(view);
    d->overlays[view] = overlay;
    overlay->setAcceptor([this, view](const DropTarget &target, const DragPayload &payload) {
        const auto resolved = d->resolveDrop(view, target, payload);
        return resolved && d->planDrop(resolved->first, resolved->second, payload).has_value();
    });
    connect(overlay, &DropOverlay::dropped, this, [this, view](const DropTarget &target, const DragPayload &payload) {
        d->onDropped(view, target, payload);
    });
}

void LayoutEditor::showItemMenu(LayoutView *view, const QString &taskId, const QString &itemId,
                                const QPoint &globalPos)
{
    if (!d->model.item(taskId, itemId))
        return;
    QMenu menu(view);
    Commands commands;
    d->addItemCommands(menu, commands, view->cellAt({}), taskId, itemId);
    if (!commands.empty())
        d->execMenu(menu, commands, globalPos);
}

void LayoutEditor::detach(LayoutView *view)
{
    if (d->views.count(view) == 0)
        return;
    disconnect(view, nullptr, this, nullptr);
    const auto overlay = d->overlays.find(view);
    if (overlay != d->overlays.end())
        delete overlay->second.data();
    d->forget(view);
}

void LayoutEditor::cellCommand(const DashboardRef &dashboard, const NodePath &path, CellCommand command)
{
    const LayoutNode *tree = d->model.layout(dashboard);
    const LayoutNode *node = tree ? layout::nodeAt(*tree, path) : nullptr;
    if (!node)
        return;

    LayoutNode edited;
    switch (command) {
    case CellCommand::SplitRight:
        edited = layout::splitCell(*tree, path, Edge::Right, std::nullopt).tree;
        break;
    case CellCommand::SplitDown:
        edited = layout::splitCell(*tree, path, Edge::Bottom, std::nullopt).tree;
        break;
    case CellCommand::WrapInTabs:
        if (isTab(*tree, path))
            return;
        edited = layout::wrapInTabs(*tree, path, d->titleFor(dashboard, node)).tree;
        break;
    case CellCommand::AddTab: {
        const EditResult result = layout::addTab(*tree, path, std::nullopt, tr("New tab"));
        edited = d->nameUntitledTabs(dashboard, result.tree, result.path);
        break;
    }
    case CellCommand::ClearCell:
        edited = layout::setRef(*tree, path, std::nullopt);
        break;
    case CellCommand::RemoveCell:
        edited = layout::removeNode(*tree, path);
        break;
    }
    d->model.setLayout(dashboard, edited);
}

void LayoutEditor::changeItem(const DashboardRef &dashboard, const NodePath &path, const CellRef &ref)
{
    const LayoutNode *tree = d->model.layout(dashboard);
    const LayoutNode *node = tree ? layout::nodeAt(*tree, path) : nullptr;
    std::optional<CellRef> stored;
    if (!node || node->kind != Kind::Cell || !d->accepts(dashboard, ref, &stored))
        return;
    d->model.setLayout(dashboard, layout::setRef(*tree, path, stored));
}

void LayoutEditor::tabCommand(const DashboardRef &dashboard, const NodePath &tabsPath, int index, TabCommand command)
{
    const LayoutNode *tree = d->model.layout(dashboard);
    const LayoutNode *tabs = tree ? layout::nodeAt(*tree, tabsPath) : nullptr;
    if (!tabs || tabs->kind != Kind::Tabs || index < 0 || index >= int(tabs->children.size()))
        return;

    LayoutNode edited;
    switch (command) {
    case TabCommand::AddTab:
        edited = layout::insertTab(*tree, tabsPath, index + 1, std::nullopt, tr("New tab")).tree;
        break;
    case TabCommand::Delete:
        edited = layout::removeNode(*tree, childOf(tabsPath, index));
        break;
    case TabCommand::MoveLeft:
        if (index == 0)
            return;
        edited = layout::moveTab(*tree, tabsPath, index, index - 1);
        break;
    case TabCommand::MoveRight:
        if (index + 1 >= int(tabs->children.size()))
            return;
        edited = layout::moveTab(*tree, tabsPath, index, index + 1);
        break;
    case TabCommand::Ungroup:
        // Normalisation replaces a collapsing group of one tab by its content.
        if (tabs->children.size() != 1)
            return;
        edited = layout::setCollapseSingle(*tree, tabsPath, true);
        break;
    }
    d->model.setLayout(dashboard, edited);
}

void LayoutEditor::renameTab(const DashboardRef &dashboard, const NodePath &tabsPath, int index,
                             const QString &title)
{
    const LayoutNode *tree = d->model.layout(dashboard);
    if (!tree)
        return;
    d->model.setLayout(dashboard, layout::renameTab(*tree, tabsPath, index, title));
}

bool LayoutEditor::drop(const DashboardRef &dashboard, const DropTarget &target, const DragPayload &payload)
{
    // Both trees are made before either is stored, since storing replaces them.
    const std::optional<Private::DropPlan> plan = d->planDrop(dashboard, target, payload);
    if (!plan)
        return false;
    if (plan->source)
        d->model.setLayout(*plan->source, plan->sourceTree);
    d->model.setLayout(plan->target, plan->tree);
    return true;
}

std::optional<LayoutEditor::Private::DropPlan>
LayoutEditor::Private::planDrop(const DashboardRef &dashboard, const DropTarget &target,
                                const DragPayload &payload) const
{
    const LayoutNode *current = model.layout(dashboard);
    if (!current)
        return std::nullopt;
    LayoutNode marked = *current;
    markRemoved(dashboard, marked);
    const LayoutNode *tree = &marked;

    const bool isMove = payload.sourceDashboard && payload.sourcePath;
    if (isMove && *payload.sourceDashboard == dashboard) {
        const EditResult result = layout::moveNode(*tree, *payload.sourcePath, target.path, target.zone);
        // A failed move returns the root path; a done one never does.
        if (result.path.empty())
            return std::nullopt;
        return DropPlan{std::nullopt, {}, dashboard, nameUntitledTabs(dashboard, result.tree, result.path)};
    }

    std::optional<CellRef> stored;
    if (!accepts(dashboard, payload.ref, &stored))
        return std::nullopt;

    // A cell moved from another dashboard leaves its source, keeping its tab title.
    DropPlan plan;
    plan.target = dashboard;
    QString title;
    if (isMove) {
        const LayoutNode *source = model.layout(*payload.sourceDashboard);
        const LayoutNode *node = source ? layout::nodeAt(*source, *payload.sourcePath) : nullptr;
        if (!node || node->kind != Kind::Cell || node->ref.value_or(CellRef{}) != payload.ref)
            return std::nullopt;
        if (const std::optional<NodePath> parentPath = parentOf(*payload.sourcePath)) {
            const LayoutNode *parent = layout::nodeAt(*source, *parentPath);
            const auto index = std::size_t(payload.sourcePath->back());
            if (parent->kind == Kind::Tabs && index < parent->titles.size())
                title = parent->titles[index];
        }
        plan.source = *payload.sourceDashboard;
        plan.sourceTree = layout::removeNode(*source, *payload.sourcePath);
    }

    LayoutNode cell;
    cell.ref = stored;
    if (title.isEmpty())
        title = titleFor(dashboard, &cell);

    const std::optional<EditResult> result = insert(dashboard, *tree, target, stored, title);
    if (!result)
        return std::nullopt;
    plan.tree = result->tree;
    return plan;
}

void LayoutEditor::setConfirm(std::function<bool(QWidget *parent, const QString &question)> confirm)
{
    d->confirm = std::move(confirm);
}

void LayoutEditor::setServices(const LayoutServices *services) { d->services = services; }

} // namespace mirante
