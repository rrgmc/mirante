#include "ui/Sidebar.h"

#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"
#include "ui/SidebarTree.h"

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QSet>
#include <QTabBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <functional>
#include <utility>
#include <vector>

namespace mirante {

namespace {

// The task lists, in the order of their tabs.
enum TaskList { ProjectsList, ActiveList, DoneList, TrashList };

} // namespace

struct Sidebar::Private
{
    SidebarModel *model = nullptr;
    QTreeView *tree = nullptr;
    QToolButton *priorityFilter = nullptr;          // its menu shows or hides each priority
    std::vector<std::pair<Priority, QAction *>> shownPriorities;
    QTabBar *stateTabs = nullptr;       // the task lists: "Projects", "Active", "Done" and "Trash"
    QPushButton *emptyTrash = nullptr;  // shown while the Trash list is current
    QHBoxLayout *header = nullptr;
    MenuFiller menuFiller;
    MenuFiller trashMenuFiller;

    // Expansion survives a reset of the model. Groups and projects start
    // expanded, so the user's collapsed ones are kept; tasks start collapsed.
    QSet<QString> collapsed;
    QSet<QString> expandedTasks;
    QString currentId;
    QString hoverId;                    // what the hovered dashboard cell shows

    // Tints the row of hoverId or, if a collapsed row hides it, that row: the
    // nearest row the user can see. Nothing if it is not in the tree.
    void applyHover()
    {
        auto *delegate = qobject_cast<SidebarDelegate *>(tree->itemDelegate());
        if (!delegate)
            return;
        QString row;
        const QModelIndex index = hoverId.isEmpty() ? QModelIndex() : model->indexOf(hoverId);
        if (index.isValid()) {
            std::vector<QModelIndex> ancestors;
            for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
                ancestors.push_back(parent);
            row = hoverId;
            for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
                if (!tree->isExpanded(*it)) {
                    // A collapsed Dashboards or Items row stands for its task.
                    const SidebarModel::Kind k = kind(*it);
                    const bool list = k == SidebarModel::Kind::ItemList || k == SidebarModel::Kind::DashboardList;
                    row = id(list ? it->parent() : *it);
                    break;
                }
            }
        }
        if (delegate->hoverId() == row)
            return;
        delegate->setHoverId(row);
        tree->viewport()->update();
    }

    SidebarModel::Kind kind(const QModelIndex &index) const
    {
        return SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
    }

    QString id(const QModelIndex &index) const { return index.data(SidebarModel::IdRole).toString(); }

    void restoreExpansion(const QModelIndex &parent)
    {
        const int rows = model->rowCount(parent);
        for (int row = 0; row < rows; ++row) {
            const QModelIndex index = model->index(row, 0, parent);
            const SidebarModel::Kind k = kind(index);
            if (k == SidebarModel::Kind::Item || k == SidebarModel::Kind::Dashboard)
                continue;
            const QString entityId = id(index);
            const bool expand = k == SidebarModel::Kind::Task ? expandedTasks.contains(entityId)
                                                               : !collapsed.contains(entityId);
            if (expand)
                tree->expand(index);
            restoreExpansion(index);
        }
    }

    // Calls fn for every task row under parent. Tasks sit under projects, so
    // the walk stops at them.
    void forEachTask(const QModelIndex &parent, const std::function<void(const QModelIndex &)> &fn)
    {
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const QModelIndex index = model->index(row, 0, parent);
            if (kind(index) == SidebarModel::Kind::Task)
                fn(index);
            else
                forEachTask(index, fn);
        }
    }

    void applyFilter()
    {
        SortFilter filter = model->filter();
        filter.hiddenPriorities.clear();
        for (const auto &[priority, action] : shownPriorities) {
            if (!action->isChecked())
                filter.hiddenPriorities.insert(priority);
        }
        // The button looks pressed while it hides something.
        priorityFilter->setChecked(!filter.hiddenPriorities.empty());
        filter.closedTasks = stateTabs->currentIndex() == DoneList;
        filter.hideEmptyProjects = stateTabs->currentIndex() == ActiveList;
        filter.trash = stateTabs->currentIndex() == TrashList;
        model->setFilter(filter);
        // Reordering is only for the other lists; the model refuses drops in the trash.
        tree->setDragEnabled(!filter.trash);
        emptyTrash->setVisible(filter.trash);
        emptyTrash->setEnabled(model->rowCount() > 0);
    }
};

Sidebar::Sidebar(const Model &model, const ItemTypeRegistry &types, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    // A light grey behind the whole sidebar, a little darker than the window,
    // as in macOS sidebars. The tree takes it as its background.
    QPalette palette = this->palette();
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const QColor window = palette.color(group, QPalette::Window);
        const QColor text = palette.color(group, QPalette::WindowText);
        const qreal amount = 0.04;
        palette.setColor(group, QPalette::Window,
                         QColor::fromRgbF(float(window.redF() + (text.redF() - window.redF()) * amount),
                                          float(window.greenF() + (text.greenF() - window.greenF()) * amount),
                                          float(window.blueF() + (text.blueF() - window.blueF()) * amount)));
    }
    setPalette(palette);
    setAutoFillBackground(true);

    d->model = new SidebarModel(model, types, this);

    // Header: the buttons, and the collapse button at the right. The
    // workspace's name is in the window title.
    auto *collapse = new QToolButton(this);
    collapse->setText(QStringLiteral("«"));
    collapse->setToolTip(tr("Hide sidebar"));
    collapse->setAutoRaise(true);
    connect(collapse, &QToolButton::clicked, this, &Sidebar::collapseRequested);

    // Expand or collapse every task at once. Groups and projects are left as
    // they are; they open and close by hand only.
    auto *expandTasks = new QToolButton(this);
    expandTasks->setObjectName(QStringLiteral("expandAllTasks"));
    expandTasks->setIcon(QIcon(QStringLiteral(":/icons/expand-all.svg")));
    expandTasks->setText(tr("Expand all tasks"));
    expandTasks->setToolTip(tr("Expand all tasks"));
    expandTasks->setAutoRaise(true);
    connect(expandTasks, &QToolButton::clicked, this, &Sidebar::expandAllTasks);
    auto *collapseTasks = new QToolButton(this);
    collapseTasks->setObjectName(QStringLiteral("collapseAllTasks"));
    collapseTasks->setIcon(QIcon(QStringLiteral(":/icons/collapse-all.svg")));
    collapseTasks->setText(tr("Collapse all tasks"));
    collapseTasks->setToolTip(tr("Collapse all tasks"));
    collapseTasks->setAutoRaise(true);
    connect(collapseTasks, &QToolButton::clicked, this, &Sidebar::collapseAllTasks);

    // Which priorities the tree shows, right of the title: a menu with a
    // checkbox for each level.
    d->priorityFilter = new QToolButton(this);
    d->priorityFilter->setObjectName(QStringLiteral("priorityFilter"));
    d->priorityFilter->setIcon(QIcon(QStringLiteral(":/icons/filter.svg")));
    d->priorityFilter->setText(tr("Priorities shown"));
    d->priorityFilter->setToolTip(tr("Choose which priorities the tree shows"));
    d->priorityFilter->setAutoRaise(true);
    d->priorityFilter->setCheckable(true);
    d->priorityFilter->setPopupMode(QToolButton::InstantPopup);
    auto *priorityMenu = new QMenu(d->priorityFilter);
    const std::pair<Priority, QString> levels[] = {{Priority::Urgent, tr("Urgent")},
                                                   {Priority::High, tr("High")},
                                                   {Priority::Normal, tr("Normal")},
                                                   {Priority::Low, tr("Low")}};
    for (const auto &[priority, name] : levels) {
        QAction *action = priorityMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(d->model->filter().hiddenPriorities.count(priority) == 0);
        connect(action, &QAction::toggled, this, [this] { d->applyFilter(); });
        d->shownPriorities.emplace_back(priority, action);
    }
    d->priorityFilter->setMenu(priorityMenu);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(8, 4, 4, 4);
    header->addWidget(d->priorityFilter);
    header->addWidget(expandTasks);
    header->addWidget(collapseTasks);
    header->addStretch(1);
    header->addWidget(collapse);
    d->header = header;

    // The tree.
    auto *tree = new SidebarTree(this);
    d->tree = tree;
    d->tree->setModel(d->model);
    d->tree->setItemDelegate(new SidebarDelegate(d->tree));
    d->tree->setHeaderHidden(true);
    // The selection shows the dashboard on screen, and the tree rarely has the
    // focus while the user works in it, so it keeps its full colour.
    QPalette treePalette = d->tree->palette();
    for (const QPalette::ColorRole role : {QPalette::Highlight, QPalette::HighlightedText})
        treePalette.setColor(QPalette::Inactive, role, treePalette.color(QPalette::Active, role));
    d->tree->setPalette(treePalette);
    d->tree->setFrameShape(QFrame::NoFrame);
    d->tree->setSelectionMode(QAbstractItemView::SingleSelection);
    // Items and dashboards are dragged onto dashboards. Every row but the two
    // lists of a task is dragged among its siblings; the model reports the drop.
    d->tree->setDragEnabled(true);
    d->tree->setDragDropMode(QAbstractItemView::DragDrop);
    d->tree->setDropIndicatorShown(true);
    d->tree->setExpandsOnDoubleClick(false);    // the sidebar handles double clicks itself
    d->tree->setContextMenuPolicy(Qt::CustomContextMenu);
    // Images dropped from other apps. See docs/decisions/0085-drop-images-from-other-apps.md.
    connect(tree, &SidebarTree::imageDropped, this, &Sidebar::imageDropped);
    // Other files dropped from other apps. See docs/decisions/0104-document-items.md.
    connect(tree, &SidebarTree::fileDropped, this, &Sidebar::fileDropped);

    // Footer: four task lists over the same groups: active tasks under every
    // project, active tasks only under projects that have some, done and
    // archived tasks, and what is in the trash.
    d->stateTabs = new QTabBar(this);
    d->stateTabs->setObjectName(QStringLiteral("taskStateTabs"));
    d->stateTabs->setShape(QTabBar::RoundedSouth);
    d->stateTabs->setDrawBase(false);
    d->stateTabs->setExpanding(false);
    d->stateTabs->addTab(tr("Projects"));
    d->stateTabs->addTab(tr("Active"));
    d->stateTabs->addTab(tr("Done"));
    d->stateTabs->addTab(tr("Trash"));
    d->stateTabs->setTabToolTip(ProjectsList, tr("Active tasks, under every project, including projects without any"));
    d->stateTabs->setTabToolTip(ActiveList, tr("Active tasks, only under projects that have some"));
    d->stateTabs->setTabToolTip(DoneList, tr("Done and archived tasks"));
    d->stateTabs->setTabToolTip(TrashList, tr("Deleted projects and tasks, which can be restored"));
    d->stateTabs->setCurrentIndex(ProjectsList);

    d->emptyTrash = new QPushButton(tr("Empty Trash…"), this);
    d->emptyTrash->setObjectName(QStringLiteral("emptyTrash"));
    d->emptyTrash->setToolTip(tr("Delete everything in the trash permanently"));
    d->emptyTrash->setVisible(false);
    connect(d->emptyTrash, &QPushButton::clicked, this, &Sidebar::emptyTrashRequested);

    connect(d->stateTabs, &QTabBar::currentChanged, this, [this] { d->applyFilter(); });

    // The button has a row of its own above the tabs, so the four tabs keep
    // their room in a narrow sidebar.
    auto *trashRow = new QHBoxLayout;
    trashRow->setContentsMargins(4, 4, 4, 0);
    trashRow->addStretch(1);
    trashRow->addWidget(d->emptyTrash);

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(4, 0, 4, 4);
    footer->addWidget(d->stateTabs);
    footer->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(d->tree, 1);
    layout->addLayout(trashRow);
    layout->addLayout(footer);

    // Expansion and the current row across resets.
    connect(d->tree, &QTreeView::collapsed, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            d->expandedTasks.remove(d->id(index));
        else
            d->collapsed.insert(d->id(index));
        d->applyHover();
    });
    connect(d->tree, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            d->expandedTasks.insert(d->id(index));
        else
            d->collapsed.remove(d->id(index));
        d->applyHover();
    });
    connect(d->model, &QAbstractItemModel::modelAboutToBeReset, this,
            [this] { d->currentId = d->id(d->tree->currentIndex()); });
    connect(d->model, &QAbstractItemModel::modelReset, this, [this] {
        d->emptyTrash->setEnabled(d->model->rowCount() > 0);
        d->restoreExpansion({});
        const QModelIndex current = d->model->indexOf(d->currentId);
        if (current.isValid())
            d->tree->setCurrentIndex(current);
        d->applyHover();
    });
    d->restoreExpansion({});

    // Activation is a double click or Enter. It opens items and dashboards,
    // and expands or collapses every other row, tasks included. Nothing in the
    // trash opens.
    connect(d->tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
        const QString taskId = index.data(SidebarModel::TaskIdRole).toString();
        const SidebarModel::Kind kind = showsTrash() ? SidebarModel::Kind::Group : d->kind(index);
        switch (kind) {
        case SidebarModel::Kind::Item:
            emit itemOpened(taskId, d->id(index));
            break;
        case SidebarModel::Kind::Dashboard:
            emit dashboardOpened(taskId, d->id(index));
            break;
        default:
            d->tree->setExpanded(index, !d->tree->isExpanded(index));
            break;
        }
    });

    connect(d->tree, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex index = d->tree->indexAt(pos);
        if (!index.isValid())
            return;
        const SidebarModel::Kind kind = d->kind(index);
        const QString taskId = index.data(SidebarModel::TaskIdRole).toString();
        const QString id = d->id(index);
        QMenu menu(this);
        if (showsTrash()) {
            if (d->trashMenuFiller)
                d->trashMenuFiller(&menu, kind, id, taskId);
        } else if (d->menuFiller) {
            // The entity commands first, then the sidebar's own.
            d->menuFiller(&menu, kind, id, taskId);
        }
        // Task dashboards and items can each be put in a new root dashboard.
        if (!showsTrash() && (kind == SidebarModel::Kind::Dashboard || kind == SidebarModel::Kind::Item)) {
            if (!menu.isEmpty())
                menu.addSeparator();
            menu.addAction(tr("Open in new root dashboard"), this, [this, kind, taskId, id] {
                if (kind == SidebarModel::Kind::Dashboard)
                    emit openInNewRootDashboard(taskId, id);
                else
                    emit openItemInNewRootDashboard(taskId, id);
            });
        }
        if (menu.isEmpty())
            return;
        menu.exec(d->tree->viewport()->mapToGlobal(pos));
    });
}

Sidebar::~Sidebar()
{
    // The children outlive d, so their signals must not reach it.
    d->stateTabs->disconnect(this);
    d->emptyTrash->disconnect(this);
    for (const auto &[priority, action] : d->shownPriorities)
        action->disconnect(this);
    d->tree->disconnect(this);
    d->model->disconnect(this);
}

void Sidebar::reveal(const QString &id)
{
    // The entry may be in another task list: try the current one, then the others.
    QModelIndex index = d->model->indexOf(id);
    const int current = d->stateTabs->currentIndex();
    for (int list = 0; !index.isValid() && list < d->stateTabs->count(); ++list) {
        if (list == current)
            continue;
        d->stateTabs->setCurrentIndex(list);
        index = d->model->indexOf(id);
    }
    if (!index.isValid()) {
        d->stateTabs->setCurrentIndex(current);
        return;
    }
    for (QModelIndex parent = index.parent(); parent.isValid(); parent = parent.parent())
        d->tree->expand(parent);
    d->tree->setCurrentIndex(index);
    d->tree->scrollTo(index);
}

void Sidebar::expandAllTasks()
{
    d->forEachTask({}, [this](const QModelIndex &task) {
        d->tree->expand(task);
        // The Dashboards and Items lists under it open too.
        for (int row = 0; row < d->model->rowCount(task); ++row)
            d->tree->expand(d->model->index(row, 0, task));
    });
}

void Sidebar::collapseAllTasks()
{
    d->forEachTask({}, [this](const QModelIndex &task) { d->tree->collapse(task); });
}

SidebarModel *Sidebar::model() const { return d->model; }

QModelIndex Sidebar::currentIndex() const { return d->tree->currentIndex(); }

void Sidebar::addHeaderWidget(QWidget *widget)
{
    // First, before the filter.
    d->header->insertWidget(0, widget);
}

void Sidebar::setMenuFiller(MenuFiller filler) { d->menuFiller = std::move(filler); }

void Sidebar::setTrashMenuFiller(MenuFiller filler) { d->trashMenuFiller = std::move(filler); }

bool Sidebar::showsTrash() const { return d->stateTabs->currentIndex() == TrashList; }

void Sidebar::focusTree()
{
    if (!d->tree->currentIndex().isValid() && d->tree->model()->rowCount() > 0)
        d->tree->setCurrentIndex(d->tree->model()->index(0, 0));
    d->tree->setFocus(Qt::ShortcutFocusReason);
}

// SidebarTree paints the tint. See SidebarDelegate::isHoverTinted().
void Sidebar::setHoverHighlight(const QString &id)
{
    if (d->hoverId == id)
        return;
    d->hoverId = id;
    d->applyHover();
}

QString Sidebar::hoverHighlight() const { return d->hoverId; }

QString Sidebar::hoverRow() const
{
    const auto *delegate = qobject_cast<const SidebarDelegate *>(d->tree->itemDelegate());
    return delegate ? delegate->hoverId() : QString();
}

} // namespace mirante
