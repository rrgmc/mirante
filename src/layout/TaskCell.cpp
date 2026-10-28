#include "layout/TaskCell.h"

#include "layout/DashboardCellFactory.h"
#include "layout/LayoutEditor.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"

#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSignalBlocker>
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
    QLabel *title = nullptr;
    QComboBox *picker = nullptr;
    QToolButton *lockButton = nullptr;
    QToolButton *openButton = nullptr;
    QLabel *removedLabel = nullptr;
    LayoutView *view = nullptr;

    const Task *task() const { return services.model ? services.model->task(taskId) : nullptr; }
    bool isUnlocked() const { return services.locks && services.locks->isUnlocked(dashboardId); }

    void refresh();
    void updatePicker(const Task &task);
    void updateLock();
    void buildView();
    void dropView();
    void showTree();
    void choose(const QString &id);
    void storeChoice();
};

// Brings the cell in line with the model: title, picker, chosen dashboard and
// the nested tree.
void TaskCell::Private::refresh()
{
    const Task *current = task();
    removed = current == nullptr;
    // A removed task keeps its header, so the cell can still be moved or
    // removed in the root dashboard.
    removedLabel->setVisible(removed);
    lockButton->setVisible(!removed && services.locks);
    openButton->setVisible(!removed && services.openTaskAlone);
    if (removed) {
        title->setText(TaskCell::tr("Removed task"));
        picker->hide();
        dropView();
        return;
    }

    title->setText(current->title);
    const QString chosen = chooseDashboard(*current, dashboardId);
    const bool rebuild = !view || chosen != dashboardId;
    dashboardId = chosen;
    updatePicker(*current);
    if (rebuild) {
        buildView();
        updateLock();
    } else {
        showTree();
    }
}

void TaskCell::Private::updatePicker(const Task &task)
{
    const QSignalBlocker blocker(picker);
    picker->clear();
    for (const Dashboard &dashboard : task.dashboards)
        picker->addItem(dashboard.name, dashboard.id);
    picker->setCurrentIndex(picker->findData(dashboardId));
    picker->setVisible(task.dashboards.size() > 1);
}

void TaskCell::Private::updateLock()
{
    const bool unlocked = isUnlocked();
    {
        const QSignalBlocker blocker(lockButton);
        lockButton->setChecked(unlocked);
    }
    lockButton->setText(unlocked ? TaskCell::tr("Unlocked") : TaskCell::tr("Locked"));
    lockButton->setToolTip(unlocked ? TaskCell::tr("Lock the task dashboard")
                                    : TaskCell::tr("Unlock the task dashboard for editing"));
    if (view)
        view->setEditMode(unlocked);
}

// Replaces the nested view with one for the chosen dashboard.
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
    view->setEditMode(isUnlocked());
    showTree();
    if (services.editor)
        services.editor->attach(view, dashboard);
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

// The user picked another dashboard.
void TaskCell::Private::choose(const QString &id)
{
    if (id.isEmpty() || id == dashboardId || removed)
        return;
    dashboardId = id;
    buildView();
    updateLock();
    storeChoice();
}

// Writes the chosen dashboard into the cell's reference in the root dashboard.
// The write is queued, because the root view may replace this cell when the
// root tree changes, and the picker's signal is still being delivered.
void TaskCell::Private::storeChoice()
{
    if (!services.model || !rootDashboard.isRoot())
        return;
    LayoutView *outer = nullptr;
    for (QWidget *parent = q->parentWidget(); parent && !outer; parent = parent->parentWidget())
        outer = qobject_cast<LayoutView *>(parent);
    if (!outer)
        return;
    const std::optional<NodePath> path = outer->pathOf(q);
    if (!path)
        return;

    Model *model = services.model;
    const QString rootId = rootDashboard.dashboardId;
    const CellRef ref{QString(), taskId, dashboardId};
    const NodePath cellPath = *path;
    QTimer::singleShot(0, model, [model, rootId, ref, cellPath] {
        const LayoutNode *tree = model->layout({QString(), rootId});
        if (!tree)
            return;
        const LayoutNode *node = layout::nodeAt(*tree, cellPath);
        if (!node || node->kind != LayoutNode::Kind::Cell || !node->ref || !node->ref->isTask()
            || node->ref->task != ref.task || *node->ref == ref)
            return;
        model->setRootDashboardLayout(rootId, layout::setRef(*tree, cellPath, ref));
    });
}

TaskCell::TaskCell(const LayoutServices &services, const DashboardRef &rootDashboard, const CellRef &ref,
                   QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->services = services;
    d->rootDashboard = rootDashboard;
    d->taskId = ref.task;
    d->dashboardId = ref.dashboard;

    d->layout = new QVBoxLayout(this);
    d->layout->setContentsMargins(0, 0, 0, 0);
    d->layout->setSpacing(0);

    d->header = new TaskHeader(this);
    d->header->setObjectName(QStringLiteral("taskHeader"));
    d->header->onDrag = [this] { emit dragStarted(); };
    d->header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d->header, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) { emit menuRequested(d->header->mapToGlobal(pos)); });
    auto *headerLayout = new QHBoxLayout(d->header);
    headerLayout->setContentsMargins(6, 2, 2, 2);
    headerLayout->setSpacing(4);

    d->title = new QLabel(d->header);
    d->title->setObjectName(QStringLiteral("taskTitle"));
    QFont font = d->title->font();
    font.setBold(true);
    d->title->setFont(font);
    headerLayout->addWidget(d->title, 1);

    d->picker = new QComboBox(d->header);
    d->picker->setObjectName(QStringLiteral("dashboardPicker"));
    d->picker->setToolTip(tr("Task dashboard"));
    d->picker->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    headerLayout->addWidget(d->picker);

    d->lockButton = new QToolButton(d->header);
    d->lockButton->setObjectName(QStringLiteral("lockButton"));
    d->lockButton->setCheckable(true);
    d->lockButton->setAutoRaise(true);
    headerLayout->addWidget(d->lockButton);

    d->openButton = new QToolButton(d->header);
    d->openButton->setObjectName(QStringLiteral("openAloneButton"));
    d->openButton->setText(tr("Open alone"));
    d->openButton->setToolTip(tr("Open this task on its own"));
    d->openButton->setAutoRaise(true);
    headerLayout->addWidget(d->openButton);

    d->layout->addWidget(d->header);

    d->removedLabel = new QLabel(tr("Removed"), this);
    d->removedLabel->setObjectName(QStringLiteral("removedLabel"));
    d->removedLabel->setAlignment(Qt::AlignCenter);
    d->removedLabel->setEnabled(false);
    d->layout->addWidget(d->removedLabel, 1);

    connect(d->picker, &QComboBox::currentIndexChanged, this,
            [this](int index) { d->choose(d->picker->itemData(index).toString()); });
    connect(d->lockButton, &QToolButton::toggled, this, [this](bool checked) {
        if (d->services.locks)
            d->services.locks->setUnlocked(d->dashboardId, checked);
        d->updateLock();
    });
    connect(d->openButton, &QToolButton::clicked, this, [this] {
        if (d->services.openTaskAlone)
            d->services.openTaskAlone(d->taskId);
    });

    if (services.locks) {
        connect(services.locks, &LockState::changed, this, [this](const QString &dashboardId, bool) {
            if (dashboardId == d->dashboardId)
                d->updateLock();
        });
    }
    if (services.model) {
        connect(services.model, &Model::taskChanged, this, [this](const QString &taskId) {
            if (taskId == d->taskId)
                d->refresh();
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
    d->picker->disconnect(this);
    d->lockButton->disconnect(this);
    d->openButton->disconnect(this);
}

QString TaskCell::taskId() const { return d->taskId; }
QString TaskCell::dashboardId() const { return d->dashboardId; }
LayoutView *TaskCell::view() const { return d->view; }

void TaskCell::setEditMode(bool on)
{
    d->header->editMode = on;
    if (on)
        d->header->setCursor(Qt::OpenHandCursor);
    else
        d->header->unsetCursor();
}

} // namespace mirante
