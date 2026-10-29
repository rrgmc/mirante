#include "ui/Sidebar.h"

#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
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

// Reports double clicks on the title label.
class TitleClicks : public QObject
{
public:
    TitleClicks(QObject *parent, std::function<void()> onDoubleClick)
        : QObject(parent)
        , m_onDoubleClick(std::move(onDoubleClick))
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonDblClick && m_onDoubleClick) {
            m_onDoubleClick();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_onDoubleClick;
};

} // namespace

struct Sidebar::Private
{
    SidebarModel *model = nullptr;
    QTreeView *tree = nullptr;
    QToolButton *priorityFilter = nullptr;          // its menu shows or hides each priority
    std::vector<std::pair<Priority, QAction *>> shownPriorities;
    QLabel *title = nullptr;
    QTabBar *stateTabs = nullptr;       // the task lists: "Projects", "Active", "Done" and "Trash"
    QPushButton *emptyTrash = nullptr;  // shown while the Trash list is current
    QPushButton *focus = nullptr;       // the Focus entry above the tree
    QHBoxLayout *header = nullptr;
    MenuFiller menuFiller;
    MenuFiller trashMenuFiller;

    // Expansion survives a reset of the model. Groups and projects start
    // expanded, so the user's collapsed ones are kept; tasks start collapsed.
    QSet<QString> collapsed;
    QSet<QString> expandedTasks;
    QString currentId;

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
    d->model = new SidebarModel(model, types, this);

    // Header: title and the collapse button.
    auto *title = new QLabel(QStringLiteral("Mirante"), this);
    title->setObjectName(QStringLiteral("workspaceTitle"));
    title->setToolTip(tr("The workspace. Double-click to rename it."));
    title->installEventFilter(new TitleClicks(this, [this] { emit titleDoubleClicked(); }));
    d->title = title;
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);

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
    header->addWidget(title, 1);
    header->addWidget(d->priorityFilter);
    header->addWidget(expandTasks);
    header->addWidget(collapseTasks);
    header->addWidget(collapse);
    d->header = header;

    // The Focus entry, above the tree.
    d->focus = new QPushButton(tr("Focus"), this);
    d->focus->setObjectName(QStringLiteral("focusEntry"));
    d->focus->setToolTip(tr("Active tasks of every project, most important first"));
    d->focus->setFlat(true);
    d->focus->setStyleSheet(QStringLiteral("QPushButton { text-align: left; padding: 4px 8px; }"));
    connect(d->focus, &QPushButton::clicked, this, &Sidebar::focusRequested);

    // The tree.
    d->tree = new QTreeView(this);
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
    // Items and dashboards are dragged onto dashboards. Groups, projects and
    // tasks are dragged among their siblings; the model reports the drop.
    d->tree->setDragEnabled(true);
    d->tree->setDragDropMode(QAbstractItemView::DragDrop);
    d->tree->setDropIndicatorShown(true);
    d->tree->setExpandsOnDoubleClick(false);    // a double click on a task pins it
    d->tree->setContextMenuPolicy(Qt::CustomContextMenu);

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
    layout->addWidget(d->focus);
    layout->addWidget(d->tree, 1);
    layout->addLayout(trashRow);
    layout->addLayout(footer);

    // Expansion and the current row across resets.
    connect(d->tree, &QTreeView::collapsed, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            d->expandedTasks.remove(d->id(index));
        else
            d->collapsed.insert(d->id(index));
    });
    connect(d->tree, &QTreeView::expanded, this, [this](const QModelIndex &index) {
        if (d->kind(index) == SidebarModel::Kind::Task)
            d->expandedTasks.insert(d->id(index));
        else
            d->collapsed.remove(d->id(index));
    });
    connect(d->model, &QAbstractItemModel::modelAboutToBeReset, this,
            [this] { d->currentId = d->id(d->tree->currentIndex()); });
    connect(d->model, &QAbstractItemModel::modelReset, this, [this] {
        d->emptyTrash->setEnabled(d->model->rowCount() > 0);
        d->restoreExpansion({});
        const QModelIndex current = d->model->indexOf(d->currentId);
        if (current.isValid())
            d->tree->setCurrentIndex(current);
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

void Sidebar::setTitle(const QString &title) { d->title->setText(title); }

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
    // After the title, which is first.
    d->header->insertWidget(1, widget);
}

void Sidebar::setMenuFiller(MenuFiller filler) { d->menuFiller = std::move(filler); }

void Sidebar::setTrashMenuFiller(MenuFiller filler) { d->trashMenuFiller = std::move(filler); }

bool Sidebar::showsTrash() const { return d->stateTabs->currentIndex() == TrashList; }

// The delegate paints the tint. See SidebarDelegate::isHoverTinted().
void Sidebar::setHoverHighlight(const QString &id)
{
    auto *delegate = qobject_cast<SidebarDelegate *>(d->tree->itemDelegate());
    if (!delegate || delegate->hoverId() == id)
        return;
    delegate->setHoverId(id);
    d->tree->viewport()->update();
}

QString Sidebar::hoverHighlight() const
{
    const auto *delegate = qobject_cast<const SidebarDelegate *>(d->tree->itemDelegate());
    return delegate ? delegate->hoverId() : QString();
}

} // namespace mirante
