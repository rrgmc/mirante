#include "layout/TaskCell.h"

#include "core/GroupColours.h"

#include "layout/CellStyle.h"
#include "layout/DashboardCellFactory.h"
#include "layout/HeaderPill.h"
#include "layout/LayoutView.h"

#include <QIcon>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace mirante {

namespace {

// The header of a task cell. In the root dashboard's edit mode, a drag past
// the drag distance calls onDrag once per press, as in CellFrame.
class TaskHeader : public QWidget
{
public:
    using QWidget::QWidget;

    std::function<void()> onDrag;
    bool editMode = false;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressPos = event->position().toPoint();
            m_pressed = true;
            m_dragged = false;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (editMode && m_pressed && !m_dragged && (event->buttons() & Qt::LeftButton)
            && (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragged = true;
            if (onDrag)
                onDrag();
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_pressed = false;
        QWidget::mouseReleaseEvent(event);
    }

private:
    QPoint m_pressPos;
    bool m_pressed = false;
    bool m_dragged = false;
};

// Keeps a view's factory alive exactly as long as the view, so the view can be
// deleted later without the factory going first.
class FactoryHolder : public QObject
{
public:
    FactoryHolder(std::unique_ptr<CellFactory> factory, QObject *parent)
        : QObject(parent)
        , m_factory(std::move(factory))
    {
    }

private:
    std::unique_ptr<CellFactory> m_factory;
};

// The dashboard to show: wanted if the task has it, otherwise the default one,
// otherwise the first one.
QString chooseDashboard(const Task &task, const QString &wanted)
{
    QString fallback;
    for (const Dashboard &dashboard : task.dashboards) {
        if (!wanted.isEmpty() && dashboard.id == wanted)
            return wanted;
        if (dashboard.id == task.defaultDashboardId)
            fallback = dashboard.id;
    }
    if (fallback.isEmpty() && !task.dashboards.empty())
        fallback = task.dashboards.front().id;
    return fallback;
}

} // namespace

struct TaskCell::Private
{
    TaskCell *q = nullptr;
    LayoutServices services;
    DashboardRef rootDashboard;
    QString taskId;
    QString dashboardId;
    bool removed = false;

    QVBoxLayout *layout = nullptr;
    TaskHeader *header = nullptr;
    HeaderRow *row = nullptr;
    HeaderPill *title = nullptr;
    HeaderPill *dashboardPill = nullptr;
    HeaderPill *context = nullptr;
    QToolButton *openButton = nullptr;
    QLabel *removedLabel = nullptr;
    LayoutView *view = nullptr;

    const Task *task() const { return services.model ? services.model->task(taskId) : nullptr; }

    void refresh();
    void tint();
    void showMenu(const QPoint &globalPos, const NodePath *path = nullptr);
    void buildView();
    void dropView();
    void showTree();
};

// Brings the cell in line with the model: title, dashboard and the nested tree.
void TaskCell::Private::refresh()
{
    const Task *current = task();
    // A task in the trash shows like a removed one, but keeps its title.
    const bool trashed = current && services.model->taskInTrash(*current);
    removed = current == nullptr || trashed;
    // A removed task keeps its header, so the cell can still be moved or
    // removed in the root dashboard.
    removedLabel->setText(trashed ? TaskCell::tr("In trash") : TaskCell::tr("Removed"));
    removedLabel->setVisible(removed);
    openButton->setVisible(!removed && services.openTaskAlone);
    tint();
    if (removed) {
        title->setText(trashed ? current->title : TaskCell::tr("Removed task"));
        title->setToolTip(QString());
        dashboardPill->clear();
        dashboardPill->setToolTip(QString());
        context->clear();
        context->setToolTip(QString());
        row->refresh();
        dropView();
        return;
    }

    const QString chosen = chooseDashboard(*current, dashboardId);
    const bool rebuild = !view || chosen != dashboardId;
    dashboardId = chosen;
    // The cell shows one task dashboard, so its header names the task, then
    // the dashboard in a pill of its own.
    QString dashboardName;
    for (const Dashboard &dashboard : current->dashboards) {
        if (dashboard.id == dashboardId)
            dashboardName = dashboard.name;
    }
    title->setText(current->title);
    dashboardPill->setText(dashboardName);
    dashboardPill->setToolTip(dashboardName);
    // Tasks from any project share a root dashboard, so the header names the
    // task's project after the title, as item cells name where they come from.
    const Project *project = services.model->project(current->projectId);
    context->setText(project ? project->name : QString());
    context->setToolTip(context->text());
    title->setToolTip(title->text());
    row->refresh();
    if (rebuild) {
        buildView();
    } else {
        showTree();
    }
}

// The header takes a light tint of the task's group colour, so the cell shows
// which group the task belongs to. A removed task has no group, and keeps the
// highlight tint. See docs/decisions/0046-group-colours.md.
void TaskCell::Private::tint()
{
    const QString colour = !removed && services.model ? groupcolours::ofTask(*services.model, taskId) : QString();
    cellstyle::tintHeader(header, cellstyle::taskHeaderStrength, colour.isEmpty() ? QColor() : QColor(colour));
}

// The menu of a task cell while the root dashboard is locked, and of the cells
// inside it, which are never edited here. An item cell inside also offers
// "Maximise" or "Restore", and "Open Alone", as in LayoutEditor.
void TaskCell::Private::showMenu(const QPoint &globalPos, const NodePath *path)
{
    if (removed)
        return;
    QPointer<QWidget> cell;
    QString itemId;
    if (path && view) {
        const LayoutNode *node = layout::nodeAt(view->tree(), *path);
        if (node && node->kind == LayoutNode::Kind::Cell && node->ref && !node->removed && node->ref->isItem()
            && services.model && services.model->item(taskId, node->ref->item)) {
            cell = view->cellAt(*path);
            itemId = node->ref->item;
        }
    }

    QMenu menu(q);
    QAction *maximise = nullptr;
    QAction *openItem = nullptr;
    QAction *edit = nullptr;
    if (cell && services.toggleMaximised) {
        const bool maximised = services.isMaximised && services.isMaximised(cell);
        maximise = menu.addAction(maximised ? TaskCell::tr("Restore") : TaskCell::tr("Maximise"));
    }
    if (!itemId.isEmpty() && services.openItemAlone)
        openItem = menu.addAction(TaskCell::tr("Open Alone"));
    if (services.openTaskAlone) {
        if (!menu.isEmpty())
            menu.addSeparator();
        edit = menu.addAction(TaskCell::tr("Edit task dashboard"));
    }
    if (menu.isEmpty())
        return;

    // The command may move or rebuild the cell whose event handler is still
    // running, so it runs once that handler has returned.
    QPointer<TaskCell> guard(q);
    QAction *chosen = menu.exec(globalPos);
    if (!chosen || !guard)
        return;
    std::function<void()> run;
    if (chosen == maximise) {
        run = [toggle = services.toggleMaximised, cell] {
            if (cell)
                toggle(cell);
        };
    } else if (chosen == openItem) {
        run = [openAlone = services.openItemAlone, task = taskId, itemId] { openAlone(task, itemId); };
    } else if (chosen == edit) {
        run = [guard] {
            if (guard)
                guard->openAlone();
        };
    }
    if (run)
        QTimer::singleShot(0, q, run);
}

// Replaces the nested view with one for the chosen dashboard. The view is
// read-only: a task dashboard is edited on its own, not inside a root
// dashboard, so the view is never in edit mode and is not attached to the
// editor. Its cells' menus offer to open it for editing instead.
void TaskCell::Private::buildView()
{
    dropView();
    const DashboardRef dashboard{taskId, dashboardId};
    auto factory = std::make_unique<DashboardCellFactory>(services, dashboard);
    CellFactory *factoryPointer = factory.get();
    view = new LayoutView(factoryPointer, q);
    new FactoryHolder(std::move(factory), view);
    if (services.tracker)
        view->setSingleViewTracker(services.tracker);
    showTree();
    QObject::connect(view, &LayoutView::cellMenuRequested, q,
                     [this](const NodePath &path, const QPoint &globalPos) { showMenu(globalPos, &path); });
    QObject::connect(view, &LayoutView::tabMenuRequested, q,
                     [this](const NodePath &, int, const QPoint &globalPos) { showMenu(globalPos); });
    layout->addWidget(view, 1);
}

// The old view may be the sender of the signal that led here, so it is
// deleted later.
void TaskCell::Private::dropView()
{
    if (!view)
        return;
    layout->removeWidget(view);
    view->hide();
    view->deleteLater();
    view = nullptr;
}

void TaskCell::Private::showTree()
{
    if (!view)
        return;
    const LayoutNode *tree = services.model ? services.model->layout({taskId, dashboardId}) : nullptr;
    view->setTree(tree ? *tree : layout::emptyLayout());
}

TaskCell::TaskCell(const LayoutServices &services, const DashboardRef &rootDashboard, const CellRef &ref,
                   QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->services = services;
    d->rootDashboard = rootDashboard;
    setAttribute(Qt::WA_StyledBackground);     // draws the edit mode border
    d->taskId = ref.task;
    d->dashboardId = ref.dashboard;

    d->layout = new QVBoxLayout(this);
    d->layout->setContentsMargins(0, 0, 0, 0);
    d->layout->setSpacing(0);

    d->header = new TaskHeader(this);
    d->header->setObjectName(QStringLiteral("taskHeader"));
    // The tint is stronger than an item header's, since the cell holds cells.
    // refresh() gives it the group's colour.
    cellstyle::tintHeader(d->header, cellstyle::taskHeaderStrength);
    d->header->onDrag = [this] { emit dragStarted(); };
    // In the root dashboard's edit mode the header has the root cell menu,
    // which offers editing the task dashboard too. Otherwise it has the short
    // menu of the cell.
    d->header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d->header, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (d->header->editMode)
            emit menuRequested(d->header->mapToGlobal(pos));
        else
            d->showMenu(d->header->mapToGlobal(pos));
    });
    auto *headerLayout = new QHBoxLayout(d->header);
    headerLayout->setContentsMargins(6, 2, 2, 2);
    headerLayout->setSpacing(4);

    d->title = new HeaderPill;
    d->title->setObjectName(QStringLiteral("taskTitle"));
    QFont font = d->title->font();
    font.setBold(true);
    d->title->setFont(font);

    // The dashboard, with the dashboard icon.
    d->dashboardPill = new HeaderPill;
    d->dashboardPill->setObjectName(QStringLiteral("taskDashboard"));
    d->dashboardPill->setFill(HeaderPill::Fill::Neutral);
    d->dashboardPill->setIcon(QIcon(QStringLiteral(":/icons/dashboard.svg")));

    // The task's project, in the grey pill that item cells use for where an
    // item comes from. It gives way first when the header is narrow, then the
    // dashboard.
    d->context = new HeaderPill;
    d->context->setObjectName(QStringLiteral("taskContext"));
    d->context->setFill(HeaderPill::Fill::Neutral);
    d->context->setForegroundRole(QPalette::PlaceholderText);

    d->row = new HeaderRow(d->header);
    d->row->setParts({d->title, d->dashboardPill, d->context});
    d->row->setShrinkOrder({d->context, d->dashboardPill, d->title});
    headerLayout->addWidget(d->row, 1);

    d->openButton = new QToolButton(d->header);
    d->openButton->setObjectName(QStringLiteral("openAloneButton"));
    d->openButton->setIcon(QIcon(QStringLiteral(":/icons/open-alone.svg")));
    d->openButton->setText(tr("Edit task dashboard"));
    d->openButton->setToolTip(tr("Open this task dashboard on its own to edit it (or double-click the cell)"));
    d->openButton->setAutoRaise(true);
    headerLayout->addWidget(d->openButton);

    d->layout->addWidget(d->header);

    d->removedLabel = new QLabel(tr("Removed"), this);
    d->removedLabel->setObjectName(QStringLiteral("removedLabel"));
    d->removedLabel->setAlignment(Qt::AlignCenter);
    d->removedLabel->setEnabled(false);
    d->layout->addWidget(d->removedLabel, 1);

    connect(d->openButton, &QToolButton::clicked, this, &TaskCell::openAlone);
    if (services.model) {
        connect(services.model, &Model::taskChanged, this, [this](const QString &taskId) {
            if (taskId == d->taskId)
                d->refresh();
        });
        connect(services.model, &Model::projectChanged, this, [this](const QString &projectId) {
            const Task *task = d->task();
            if (task && task->projectId == projectId)
                d->refresh();
        });
        // A new group colour changes only the header's tint.
        connect(services.model, &Model::groupChanged, this, [this](const QString &groupId) {
            const Task *task = d->task();
            const Project *project = task ? d->services.model->project(task->projectId) : nullptr;
            if (project && project->groupId == groupId)
                d->tint();
        });
        connect(services.model, &Model::modelReset, this, [this] { d->refresh(); });
    }

    d->refresh();
}

TaskCell::~TaskCell()
{
    // The child widgets outlive d, so their signals must not reach it.
    d->header->onDrag = nullptr;
    d->header->disconnect(this);
    d->openButton->disconnect(this);
}

QString TaskCell::taskId() const { return d->taskId; }
QString TaskCell::dashboardId() const { return d->dashboardId; }
LayoutView *TaskCell::view() const { return d->view; }

void TaskCell::openAlone()
{
    if (!d->removed && d->services.openTaskAlone)
        d->services.openTaskAlone(d->taskId, d->dashboardId);
}

// A double click anywhere in the cell opens the task dashboard on its own.
// The cells inside do not use double clicks, so theirs arrive here.
void TaskCell::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        openAlone();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void TaskCell::setEditMode(bool on)
{
    d->header->editMode = on;
    if (on)
        d->header->setCursor(Qt::OpenHandCursor);
    else
        d->header->unsetCursor();

    // In the root dashboard's edit mode the whole task cell gets a border, so
    // it reads as one cell that can be moved or removed.
    d->layout->setContentsMargins(on ? QMargins(2, 2, 2, 2) : QMargins());
    setStyleSheet(on ? cellstyle::border(QStringLiteral("mirante--TaskCell"), palette(), true, false) : QString());
}

} // namespace mirante
