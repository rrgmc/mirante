#include "ui/MainWindow.h"

#include "Version.h"
#include "core/LayoutTree.h"
#include "core/Model.h"
#include "layout/CellFrame.h"
#include "layout/DashboardCellFactory.h"
#include "layout/LayoutEditor.h"
#include "layout/LayoutServices.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"
#include "layout/SingleViewTracker.h"
#include "layout/TaskCell.h"
#include "ui/QuickSwitcher.h"
#include "ui/RootTabBar.h"
#include "ui/Shortcuts.h"
#include "ui/Sidebar.h"

#include <QIcon>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace mirante {

namespace {

// One page of the stack: a LayoutView of one dashboard, with its factory.
class DashboardPage : public QWidget
{
public:
    DashboardPage(const LayoutServices &services, const DashboardRef &dashboard, QWidget *parent)
        : QWidget(parent)
        , ref(dashboard)
        , factory(std::make_unique<DashboardCellFactory>(services, dashboard))
    {
        view = new LayoutView(factory.get(), this);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(view);
    }

    ~DashboardPage() override
    {
        // The view goes before the factory it uses.
        delete view;
    }

    const DashboardRef ref;
    const std::unique_ptr<DashboardCellFactory> factory;
    LayoutView *view = nullptr;
};

// The visible cell that shows the item, looking inside task cells too.
QWidget *findItemCell(const LayoutView *view, const QString &itemId)
{
    for (QWidget *cell : view->visibleCells()) {
        if (auto *frame = qobject_cast<CellFrame *>(cell)) {
            const auto ref = frame->ref();
            if (ref && ref->item == itemId)
                return frame;
        } else if (auto *taskCell = qobject_cast<TaskCell *>(cell)) {
            if (const LayoutView *inner = taskCell->view()) {
                if (QWidget *found = findItemCell(inner, itemId))
                    return found;
            }
        }
    }
    return nullptr;
}

} // namespace

struct MainWindow::Private
{
    MainWindow *q = nullptr;
    Model *model = nullptr;
    const ItemTypeRegistry *types = nullptr;

    LayoutEditor *editor = nullptr;
    LockState *locks = nullptr;
    SingleViewTracker *tracker = nullptr;
    LayoutServices services;

    QSplitter *splitter = nullptr;
    Sidebar *sidebar = nullptr;
    QFrame *warningBar = nullptr;
    QLabel *warningText = nullptr;
    RootTabBar *tabBar = nullptr;
    QToolButton *showSidebarButton = nullptr;
    QToolButton *lockButton = nullptr;
    QStackedWidget *stack = nullptr;
    Shortcuts *shortcuts = nullptr;
    QuickSwitcher *switcher = nullptr;

    std::vector<DashboardPage *> pages;     // in tab order
    DashboardPage *preview = nullptr;       // also in pages, always last

    int indexOf(const DashboardPage *page) const
    {
        const auto found = std::find(pages.begin(), pages.end(), page);
        return found == pages.end() ? -1 : int(found - pages.begin());
    }

    DashboardPage *currentPage() const
    {
        if (pages.empty())
            return nullptr;
        const int index = tabBar->currentIndex();
        return index >= 0 && index < int(pages.size()) ? pages[std::size_t(index)] : nullptr;
    }

    DashboardPage *rootPage(const QString &id) const
    {
        for (DashboardPage *page : pages) {
            if (page->ref.isRoot() && page->ref.dashboardId == id)
                return page;
        }
        return nullptr;
    }

    QString title(const DashboardRef &ref) const
    {
        if (ref.isRoot()) {
            const RootDashboard *dashboard = model->rootDashboard(ref.dashboardId);
            return dashboard ? dashboard->name : QString();
        }
        const Task *task = model->task(ref.taskId);
        return task ? task->title : QString();
    }

    DashboardPage *makePage(const DashboardRef &ref)
    {
        auto *page = new DashboardPage(services, ref, stack);
        editor->attach(page->view, ref);
        page->view->setSingleViewTracker(tracker);
        if (const LayoutNode *tree = model->layout(ref))
            page->view->setTree(*tree);
        page->view->setEditMode(locks->isUnlocked(ref.dashboardId));
        stack->addWidget(page);
        return page;
    }

    void insertTab(int index, DashboardPage *page)
    {
        pages.insert(pages.begin() + index, page);
        tabBar->insertTab(index, title(page->ref));
        syncPreview();
        syncCurrent();
    }

    void removeTab(int index)
    {
        if (index < 0 || index >= int(pages.size()))
            return;
        DashboardPage *page = pages[std::size_t(index)];
        pages.erase(pages.begin() + index);
        if (preview == page)
            preview = nullptr;
        stack->removeWidget(page);
        tabBar->removeTab(index);
        delete page;
        syncPreview();
        syncCurrent();
    }

    void setCurrent(const DashboardPage *page)
    {
        const int index = indexOf(page);
        if (index >= 0)
            tabBar->setCurrentIndex(index);
        syncCurrent();
    }

    // Called after every change to the tabs. QTabBar does not update its size
    // hint while it is hidden, so the layout would keep the empty bar's size.
    void syncPreview()
    {
        tabBar->setPreviewIndex(preview ? indexOf(preview) : -1);
        tabBar->updateGeometry();
    }

    // Shows the page of the current tab, and its lock state on the toolbar.
    void syncCurrent()
    {
        DashboardPage *page = currentPage();
        if (page)
            stack->setCurrentWidget(page);
        updateLockButton();
    }

    void updateLockButton()
    {
        const DashboardPage *page = currentPage();
        const bool unlocked = page && locks->isUnlocked(page->ref.dashboardId);
        const QSignalBlocker blocker(lockButton);
        lockButton->setEnabled(page != nullptr);
        lockButton->setChecked(unlocked);
        // The button shows the icon; the text is its accessible name.
        lockButton->setIcon(QIcon(unlocked ? QStringLiteral(":/icons/lock-open.svg") : QStringLiteral(":/icons/lock-closed.svg")));
        lockButton->setText(unlocked ? tr("Editing") : tr("Locked"));
        lockButton->setToolTip(unlocked ? tr("Lock this dashboard") : tr("Unlock this dashboard for editing"));
    }

    void applyLock(const QString &dashboardId, bool unlocked)
    {
        for (DashboardPage *page : pages) {
            if (page->ref.dashboardId == dashboardId)
                page->view->setEditMode(unlocked);
        }
        updateLockButton();
    }

    // Where a root tab with this order goes: before the first open root tab
    // with a greater order, and always before the preview.
    int rootInsertIndex(int order) const
    {
        for (std::size_t i = 0; i < pages.size(); ++i) {
            const DashboardPage *page = pages[i];
            if (page == preview)
                return int(i);
            const RootDashboard *dashboard = model->rootDashboard(page->ref.dashboardId);
            if (dashboard && dashboard->order > order)
                return int(i);
        }
        return int(pages.size());
    }

    std::vector<const RootDashboard *> sortedRootDashboards() const
    {
        auto dashboards = model->rootDashboards();
        std::stable_sort(dashboards.begin(), dashboards.end(), [](const RootDashboard *a, const RootDashboard *b) {
            return a->order < b->order;
        });
        return dashboards;
    }

    // The root dashboard that pins the task: one whose layout is a single
    // cell of that task. Empty if there is none.
    QString pinnedDashboardOf(const QString &taskId) const
    {
        for (const RootDashboard *dashboard : sortedRootDashboards()) {
            const LayoutNode &tree = dashboard->layout;
            if (tree.kind == LayoutNode::Kind::Cell && tree.ref && tree.ref->isTask() && tree.ref->task == taskId)
                return dashboard->id;
        }
        return QString();
    }

    static QString defaultDashboardOf(const Task &task)
    {
        if (!task.defaultDashboardId.isEmpty())
            return task.defaultDashboardId;
        return task.dashboards.empty() ? QString() : task.dashboards.front().id;
    }

    int minOrder() const
    {
        const auto dashboards = model->rootDashboards();
        int order = std::numeric_limits<int>::max();
        for (const RootDashboard *dashboard : dashboards)
            order = std::min(order, dashboard->order);
        return dashboards.empty() ? 0 : order;
    }

    int maxOrder() const
    {
        const auto dashboards = model->rootDashboards();
        int order = std::numeric_limits<int>::min();
        for (const RootDashboard *dashboard : dashboards)
            order = std::max(order, dashboard->order);
        return dashboards.empty() ? -1 : order;
    }

    // Adds a root dashboard with one cell of the task, placed before every
    // other root dashboard. Returns its id, or an empty string.
    QString addTaskRootDashboard(const QString &taskId)
    {
        const Task *task = model->task(taskId);
        if (!task)
            return QString();
        CellRef ref;
        ref.task = task->id;
        ref.dashboard = defaultDashboardOf(*task);

        RootDashboard dashboard;
        dashboard.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        dashboard.name = task->title;
        dashboard.order = minOrder() - 1;
        dashboard.layout.ref = ref;
        model->addRootDashboard(dashboard);
        return dashboard.id;
    }

    void closePreviewOf(const QString &taskId)
    {
        if (preview && preview->ref.taskId == taskId)
            removeTab(indexOf(preview));
    }

    void openInitialTabs()
    {
        for (const RootDashboard *dashboard : sortedRootDashboards())
            insertTab(int(pages.size()), makePage(DashboardRef{QString(), dashboard->id}));
        if (!pages.empty())
            setCurrent(pages.front());
    }

    void closeAllTabs()
    {
        while (!pages.empty())
            removeTab(int(pages.size()) - 1);
    }

    void onRootDashboardChanged(const QString &id)
    {
        for (std::size_t i = 0; i < pages.size(); ++i) {
            DashboardPage *page = pages[i];
            if (!page->ref.isRoot() || page->ref.dashboardId != id)
                continue;
            if (const LayoutNode *tree = model->layout(page->ref))
                page->view->setTree(*tree);
            tabBar->setTabText(int(i), title(page->ref));
        }
        tabBar->updateGeometry();
        syncCurrent();
    }

    void onTaskChanged(const QString &taskId)
    {
        for (std::size_t i = 0; i < pages.size(); ++i) {
            DashboardPage *page = pages[i];
            if (page->ref.taskId != taskId)
                continue;
            if (const LayoutNode *tree = model->layout(page->ref))
                page->view->setTree(*tree);
            tabBar->setTabText(int(i), title(page->ref));
        }
        tabBar->updateGeometry();
        syncCurrent();

        // Editing the dashboard in the preview tab pins it (docs/ui.md). The
        // change may come from the preview's own view, so pin after it returns.
        if (preview && preview->ref.taskId == taskId && currentPage() == preview)
            QTimer::singleShot(0, q, [window = q, taskId] { window->pinTask(taskId); });
    }

    void showSidebar(bool show)
    {
        sidebar->setVisible(show);
        showSidebarButton->setVisible(!show);
    }

    void onSwitcherActivated(const SwitcherEntry &entry)
    {
        switch (entry.kind) {
        case SwitcherEntry::Kind::Task:
            q->openTaskPreview(entry.id);
            break;
        case SwitcherEntry::Kind::Item:
            q->focusItem(entry.taskId, entry.id);
            break;
        case SwitcherEntry::Kind::RootDashboard:
            q->openRootDashboard(entry.id);
            break;
        case SwitcherEntry::Kind::Group:
        case SwitcherEntry::Kind::Project:
            q->revealInSidebar(entry.id);
            break;
        }
    }

    QWidget *buildWarningBar()
    {
        warningBar = new QFrame;
        warningBar->setObjectName(QStringLiteral("warningBar"));
        warningBar->setFrameShape(QFrame::StyledPanel);
        warningBar->setAutoFillBackground(true);
        QPalette palette = warningBar->palette();
        palette.setColor(QPalette::Window, palette.color(QPalette::ToolTipBase));
        palette.setColor(QPalette::WindowText, palette.color(QPalette::ToolTipText));
        warningBar->setPalette(palette);

        warningText = new QLabel(warningBar);
        warningText->setWordWrap(true);
        warningText->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto *close = new QToolButton(warningBar);
        close->setText(QStringLiteral("✕"));
        close->setToolTip(tr("Hide this warning"));
        close->setAutoRaise(true);
        QObject::connect(close, &QToolButton::clicked, warningBar, &QWidget::hide);

        auto *layout = new QHBoxLayout(warningBar);
        layout->setContentsMargins(8, 4, 4, 4);
        layout->addWidget(warningText, 1);
        layout->addWidget(close, 0, Qt::AlignTop);
        warningBar->hide();
        return warningBar;
    }

    QWidget *buildContent()
    {
        auto *content = new QWidget;

        // The root tab bar and the + button after it.
        tabBar = new RootTabBar(content);
        auto *addButton = new QToolButton(content);
        addButton->setObjectName(QStringLiteral("newDashboardButton"));
        addButton->setText(QStringLiteral("+"));
        addButton->setToolTip(tr("New root dashboard"));
        addButton->setAutoRaise(true);
        QObject::connect(addButton, &QToolButton::clicked, tabBar, &RootTabBar::newDashboardRequested);

        auto *tabRow = new QHBoxLayout;
        tabRow->setContentsMargins(4, 4, 4, 0);
        tabRow->setSpacing(2);
        tabRow->addWidget(tabBar);
        tabRow->addWidget(addButton);
        tabRow->addStretch(1);

        // The dashboard toolbar.
        showSidebarButton = new QToolButton(content);
        showSidebarButton->setObjectName(QStringLiteral("showSidebarButton"));
        showSidebarButton->setText(QStringLiteral("»"));
        showSidebarButton->setToolTip(tr("Show sidebar"));
        showSidebarButton->setAutoRaise(true);
        showSidebarButton->hide();

        lockButton = new QToolButton(content);
        lockButton->setObjectName(QStringLiteral("lockButton"));
        lockButton->setCheckable(true);
        lockButton->setAutoRaise(true);

        auto *toolbar = new QHBoxLayout;
        toolbar->setContentsMargins(4, 2, 4, 2);
        toolbar->addWidget(showSidebarButton);
        toolbar->addStretch(1);
        toolbar->addWidget(lockButton);

        stack = new QStackedWidget(content);

        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(buildWarningBar());
        layout->addLayout(tabRow);
        layout->addLayout(toolbar);
        layout->addWidget(stack, 1);
        return content;
    }
};

MainWindow::MainWindow(Model &model, const ItemTypeRegistry &types, QWidget *parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;
    d->types = &types;

    d->editor = new LayoutEditor(model, types, this);
    d->locks = new LockState(this);
    d->tracker = new SingleViewTracker(this);
    d->services.model = &model;
    d->services.types = &types;
    d->services.editor = d->editor;
    d->services.locks = d->locks;
    d->services.tracker = d->tracker;
    d->services.openTaskAlone = [this](const QString &taskId) { openTaskPreview(taskId); };

    setWindowTitle(QStringLiteral(MIRANTE_APP_NAME));
    resize(1280, 800);
    setMinimumSize(720, 480);

    d->sidebar = new Sidebar(model, types);
    d->splitter = new QSplitter(Qt::Horizontal, this);
    d->splitter->setChildrenCollapsible(false);
    d->splitter->addWidget(d->sidebar);
    d->splitter->addWidget(d->buildContent());
    d->splitter->setStretchFactor(0, 0);
    d->splitter->setStretchFactor(1, 1);
    d->splitter->setSizes({260, 1020});
    setCentralWidget(d->splitter);

    // The sidebar.
    connect(d->sidebar, &Sidebar::taskOpened, this, &MainWindow::openTaskPreview);
    connect(d->sidebar, &Sidebar::itemOpened, this, &MainWindow::focusItem);
    connect(d->sidebar, &Sidebar::openInNewRootDashboard, this, [this](const QString &taskId) {
        const QString id = d->addTaskRootDashboard(taskId);
        if (!id.isEmpty())
            openRootDashboard(id);
    });
    connect(d->sidebar, &Sidebar::collapseRequested, this, [this] { d->showSidebar(false); });
    connect(d->showSidebarButton, &QToolButton::clicked, this, [this] { d->showSidebar(true); });

    // The tab bar.
    connect(d->tabBar, &QTabBar::currentChanged, this, [this] { d->syncCurrent(); });
    connect(d->tabBar, &QTabBar::tabCloseRequested, this, [this](int index) { d->removeTab(index); });
    connect(d->tabBar, &RootTabBar::pinRequested, this, [this](int index) {
        if (index >= 0 && index < int(d->pages.size()) && d->pages[std::size_t(index)] == d->preview)
            pinTask(d->preview->ref.taskId);
    });
    connect(d->tabBar, &RootTabBar::newDashboardRequested, this, &MainWindow::newRootDashboard);

    // Locks.
    connect(d->lockButton, &QToolButton::toggled, this, [this](bool unlocked) {
        if (const DashboardPage *page = d->currentPage()) {
            d->locks->setUnlocked(page->ref.dashboardId, unlocked);
        }
    });
    connect(d->locks, &LockState::changed, this,
            [this](const QString &id, bool unlocked) { d->applyLock(id, unlocked); });

    // The model.
    connect(&model, &Model::rootDashboardChanged, this, [this](const QString &id) { d->onRootDashboardChanged(id); });
    connect(&model, &Model::taskChanged, this, [this](const QString &taskId) { d->onTaskChanged(taskId); });
    connect(&model, &Model::modelReset, this, [this] {
        d->closeAllTabs();
        d->openInitialTabs();
    });

    // The switcher and the shortcuts.
    d->switcher = new QuickSwitcher(model, this);
    connect(d->switcher, &QuickSwitcher::activated, this,
            [this](const SwitcherEntry &entry) { d->onSwitcherActivated(entry); });
    auto *switcherShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
    switcherShortcut->setContext(Qt::WindowShortcut);
    connect(switcherShortcut, &QShortcut::activated, this, &MainWindow::showQuickSwitcher);

    d->openInitialTabs();
    d->shortcuts = new Shortcuts(this);
}

MainWindow::~MainWindow()
{
    // Lambdas connected here read d, which goes before QWidget's destructor
    // deletes the children. Disconnect them all first.
    disconnect(d->model, nullptr, this, nullptr);
    const auto children = findChildren<QObject *>();
    for (QObject *child : children)
        child->disconnect(this);

    // The pages go next, while the services they use still exist.
    d->pages.clear();
    d->preview = nullptr;
    delete d->splitter;
    d->splitter = nullptr;
    d->tabBar = nullptr;
    d->stack = nullptr;
}

void MainWindow::setLoadProblems(const std::vector<LoadProblem> &problems)
{
    if (problems.empty()) {
        d->warningText->clear();
        d->warningBar->hide();
        return;
    }
    QStringList lines;
    lines << tr("Some files could not be loaded and were skipped:");
    for (const LoadProblem &problem : problems)
        lines << tr("%1: %2").arg(problem.file, problem.message);
    d->warningText->setText(lines.join(QLatin1Char('\n')));
    d->warningBar->show();
}

void MainWindow::openTaskPreview(const QString &taskId)
{
    const Task *task = d->model->task(taskId);
    if (!task)
        return;

    const QString pinned = d->pinnedDashboardOf(taskId);
    if (!pinned.isEmpty()) {
        openRootDashboard(pinned);
        return;
    }

    const QString dashboardId = Private::defaultDashboardOf(*task);
    if (dashboardId.isEmpty())
        return;
    const DashboardRef ref{taskId, dashboardId};
    if (d->preview && d->preview->ref == ref) {
        d->setCurrent(d->preview);
        return;
    }

    DashboardPage *page = d->makePage(ref);
    if (d->preview) {
        // Replace the preview in place.
        const int index = d->indexOf(d->preview);
        DashboardPage *old = d->preview;
        d->pages[std::size_t(index)] = page;
        d->preview = page;
        d->stack->removeWidget(old);
        delete old;
        d->tabBar->setTabText(index, d->title(ref));
        d->syncPreview();
    } else {
        d->preview = page;
        d->insertTab(int(d->pages.size()), page);
    }
    d->setCurrent(page);
}

void MainWindow::pinTask(const QString &taskId)
{
    if (!d->model->task(taskId))
        return;
    QString id = d->pinnedDashboardOf(taskId);
    if (id.isEmpty())
        id = d->addTaskRootDashboard(taskId);
    d->closePreviewOf(taskId);
    openRootDashboard(id);
}

void MainWindow::openRootDashboard(const QString &id)
{
    const RootDashboard *dashboard = d->model->rootDashboard(id);
    if (!dashboard)
        return;
    DashboardPage *page = d->rootPage(id);
    if (!page) {
        const int index = d->rootInsertIndex(dashboard->order);
        page = d->makePage(DashboardRef{QString(), id});
        d->insertTab(index, page);
    }
    d->setCurrent(page);
}

void MainWindow::newRootDashboard()
{
    RootDashboard dashboard;
    dashboard.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dashboard.name = tr("New dashboard");
    dashboard.order = d->maxOrder() + 1;
    dashboard.layout = layout::emptyLayout();
    d->model->addRootDashboard(dashboard);
    d->locks->setUnlocked(dashboard.id, true);
    openRootDashboard(dashboard.id);
}

void MainWindow::focusItem(const QString &taskId, const QString &itemId)
{
    openTaskPreview(taskId);
    const DashboardPage *page = d->currentPage();
    if (!page)
        return;
    if (QWidget *cell = findItemCell(page->view, itemId))
        d->shortcuts->focusCell(cell);
}

void MainWindow::revealInSidebar(const QString &id)
{
    d->showSidebar(true);
    d->sidebar->reveal(id);
}

void MainWindow::showQuickSwitcher() { d->switcher->popup(); }

void MainWindow::closeCurrentTab() { d->removeTab(d->tabBar->currentIndex()); }

RootTabBar *MainWindow::rootTabBar() const { return d->tabBar; }

LayoutView *MainWindow::currentLayoutView() const
{
    const DashboardPage *page = d->currentPage();
    return page ? page->view : nullptr;
}

QWidget *MainWindow::contentArea() const { return d->stack; }

Shortcuts *MainWindow::shortcuts() const { return d->shortcuts; }

} // namespace mirante
