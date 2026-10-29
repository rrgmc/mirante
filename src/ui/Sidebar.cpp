#include "ui/Sidebar.h"

#include "ui/SidebarDelegate.h"
#include "ui/SidebarModel.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QTabBar>
#include <QToolButton>
#include <QToolTip>
#include <QTreeView>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace mirante {

namespace {

// The task lists, in the order of their tabs.
enum TaskList { ProjectsList, ActiveList, DoneList };

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

// The tree. While a project or task is dragged across a priority boundary, a
// tooltip says which level it will take, since the drop changes its priority.
class SidebarTree : public QTreeView
{
public:
    using QTreeView::QTreeView;

protected:
    void dragMoveEvent(QDragMoveEvent *event) override
    {
        QTreeView::dragMoveEvent(event);
        const QString hint = event->isAccepted() ? dropHint(event) : QString();
        if (hint.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(viewport()->mapToGlobal(event->position().toPoint() + QPoint(16, 16)), hint,
                               viewport());
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        QToolTip::hideText();
        QTreeView::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        QToolTip::hideText();
        QTreeView::dropEvent(event);
    }

private:
    // The row and parent of the drop, as QAbstractItemView works them out from
    // the drop indicator.
    QString dropHint(const QDropEvent *event) const
    {
        const auto *sidebarModel = qobject_cast<const SidebarModel *>(model());
        if (!sidebarModel)
            return {};
        const QModelIndex index = indexAt(event->position().toPoint());
        int row = -1;
        QModelIndex parent = index;
        switch (dropIndicatorPosition()) {
        case AboveItem:
            row = index.row();
            parent = index.parent();
            break;
        case BelowItem:
            row = index.row() + 1;
            parent = index.parent();
            break;
        case OnItem:
            break;
        case OnViewport:
            parent = rootIndex();
            break;
        }
        return sidebarModel->dropHint(event->mimeData(), row, parent);
    }
};

} // namespace

struct Sidebar::Private
{
    SidebarModel *model = nullptr;
    QTreeView *tree = nullptr;
    QToolButton *hideLow = nullptr;
    QLabel *title = nullptr;
    QTabBar *stateTabs = nullptr;       // the task lists: "Projects", "Active" and "Done"
    QPushButton *focus = nullptr;       // the Focus entry above the tree
    QHBoxLayout *header = nullptr;
    MenuFiller menuFiller;

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
        filter.hideLow = hideLow->isChecked();
        filter.closedTasks = stateTabs->currentIndex() == DoneList;
        filter.hideEmptyProjects = stateTabs->currentIndex() == ActiveList;
        model->setFilter(filter);
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

    auto *header = new QHBoxLayout;
    header->setContentsMargins(8, 4, 4, 4);
    header->addWidget(title, 1);
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
    d->tree = new SidebarTree(this);
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

    // Footer: the filters.
    d->hideLow = new QToolButton(this);
    d->hideLow->setText(tr("Hide Low"));
    d->hideLow->setToolTip(tr("Hide Low projects and tasks"));
    d->hideLow->setCheckable(true);
    d->hideLow->setChecked(d->model->filter().hideLow);
    d->hideLow->setAutoRaise(true);

    // Three task lists over the same groups: active tasks under every project,
    // active tasks only under projects that have some, and done and archived
    // tasks.
    d->stateTabs = new QTabBar(this);
    d->stateTabs->setObjectName(QStringLiteral("taskStateTabs"));
    d->stateTabs->setShape(QTabBar::RoundedSouth);
    d->stateTabs->setDrawBase(false);
    d->stateTabs->setExpanding(false);
    d->stateTabs->addTab(tr("Projects"));
    d->stateTabs->addTab(tr("Active"));
    d->stateTabs->addTab(tr("Done"));
    d->stateTabs->setTabToolTip(ProjectsList, tr("Active tasks, under every project, including projects without any"));
    d->stateTabs->setTabToolTip(ActiveList, tr("Active tasks, only under projects that have some"));
    d->stateTabs->setTabToolTip(DoneList, tr("Done and archived tasks"));
    d->stateTabs->setCurrentIndex(ProjectsList);

    connect(d->hideLow, &QToolButton::toggled, this, [this] { d->applyFilter(); });
    connect(d->stateTabs, &QTabBar::currentChanged, this, [this] { d->applyFilter(); });

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(4, 0, 4, 4);
    footer->addWidget(d->stateTabs);
    footer->addStretch(1);
    footer->addWidget(d->hideLow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(d->focus);
    layout->addWidget(d->tree, 1);
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
        d->restoreExpansion({});
        const QModelIndex current = d->model->indexOf(d->currentId);
        if (current.isValid())
            d->tree->setCurrentIndex(current);
    });
    d->restoreExpansion({});

    // Activation is a double click or Enter. It opens items and dashboards,
    // and expands or collapses every other row, tasks included.
    connect(d->tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
        const QString taskId = index.data(SidebarModel::TaskIdRole).toString();
        switch (d->kind(index)) {
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
        // The entity commands first, then the sidebar's own.
        if (d->menuFiller)
            d->menuFiller(&menu, kind, id, taskId);
        // Task dashboards and items can each be put in a new root dashboard.
        if (kind == SidebarModel::Kind::Dashboard || kind == SidebarModel::Kind::Item) {
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
    d->hideLow->disconnect(this);
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

} // namespace mirante
