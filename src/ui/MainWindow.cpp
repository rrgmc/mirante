#include "ui/MainWindow.h"

#include "Version.h"
#include "core/LayoutTree.h"
#include "core/Model.h"
#include "items/ItemTypeRegistry.h"
#include "layout/CellFrame.h"
#include "layout/CellStyle.h"
#include "layout/DashboardCellFactory.h"
#include "layout/LayoutEditor.h"
#include "layout/LayoutServices.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"
#include "layout/SingleViewTracker.h"
#include "layout/TaskCell.h"
#include "ui/QuickSwitcher.h"
#include "ui/RootDashboardsDialog.h"
#include "ui/RootTabBar.h"
#include "ui/Shortcuts.h"
#include "ui/Sidebar.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QScreen>
#include <QSettings>
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
// One tab's page. A task dashboard's page has a title bar with its own lock,
// because a task dashboard is edited on its own (decision 0020). A root
// dashboard's lock is the one at the end of the tab bar row.
class DashboardPage : public QWidget
{
public:
    // With an item id, the page shows only that item of the task, read-only.
    DashboardPage(const LayoutServices &services, const DashboardRef &dashboard, QWidget *parent,
                  const QString &item = QString())
        : QWidget(parent)
        , ref(dashboard)
        , itemId(item)
        , factory(std::make_unique<DashboardCellFactory>(services, dashboard))
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        if (!dashboard.isRoot()) {
            auto *header = new QWidget(this);
            header->setObjectName(QStringLiteral("taskDashboardHeader"));
            cellstyle::tintHeader(header, 0.22);
            auto *headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(6, 2, 2, 2);
            title = new QLabel(header);
            QFont font = title->font();
            font.setBold(true);
            title->setFont(font);
            headerLayout->addWidget(title, 1);
            if (item.isEmpty()) {
                lock = new QToolButton(header);
                lock->setObjectName(QStringLiteral("taskLockButton"));
                lock->setAutoRaise(true);
                headerLayout->addWidget(lock);
            }
            layout->addWidget(header);
        }
        view = new LayoutView(factory.get(), this);
        layout->addWidget(view, 1);
    }

    ~DashboardPage() override
    {
        // The view goes before the factory it uses.
        delete view;
    }

    const DashboardRef ref;
    const QString itemId;           // set for an item page
    const std::unique_ptr<DashboardCellFactory> factory;
    LayoutView *view = nullptr;
    QLabel *title = nullptr;        // task dashboards only
    QToolButton *lock = nullptr;    // task dashboards only
};

// Shows a lock's state on its button. The icon shows the state, so the button
// is not checkable: a checked button's accent fill would hide the icon. The
// text is its accessible name.
void showLockState(QToolButton *button, bool unlocked)
{
    button->setIcon(QIcon(unlocked ? QStringLiteral(":/icons/lock-open.svg") : QStringLiteral(":/icons/lock-closed.svg")));
    button->setText(unlocked ? MainWindow::tr("Editing") : MainWindow::tr("Locked"));
    button->setToolTip(unlocked ? MainWindow::tr("Lock this dashboard") : MainWindow::tr("Unlock this dashboard for editing"));
}

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
    QPointer<DashboardPage> shownPage;      // the page syncCurrent() last showed
    QFrame *warningBar = nullptr;
    QLabel *warningText = nullptr;
    RootTabBar *tabBar = nullptr;
    QToolButton *showSidebarButton = nullptr;
    QToolButton *lockButton = nullptr;
    QStackedWidget *stack = nullptr;
    Shortcuts *shortcuts = nullptr;
    QuickSwitcher *switcher = nullptr;
    QToolButton *listButton = nullptr;
    QPointer<RootDashboardsDialog> dashboardList;

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
        return taskDashboardTitle(ref.taskId, ref.dashboardId);
    }

    // A tab's title: the root dashboard's name, "Task / Dashboard", or for an
    // item page "Task / Item".
    QString pageTitle(const DashboardPage *page) const
    {
        if (page->itemId.isEmpty())
            return title(page->ref);
        const Task *task = model->task(page->ref.taskId);
        const Item *item = model->item(page->ref.taskId, page->itemId);
        if (!task || !item)
            return QString();
        const QString name = item->name.isEmpty() ? types->type(item->type).name() : item->name;
        return MainWindow::tr("%1 / %2").arg(task->title, name);
    }

    // Task dashboard tabs carry the dashboard icon and item tabs the item's
    // type icon, so they read apart from root dashboard tabs, which have none.
    QIcon pageIcon(const DashboardPage *page) const
    {
        if (page->ref.isRoot())
            return QIcon();
        if (page->itemId.isEmpty())
            return QIcon(QStringLiteral(":/icons/dashboard.svg"));
        const Item *item = model->item(page->ref.taskId, page->itemId);
        return item ? types->type(item->type).icon() : QIcon();
    }

    // Puts a new page in the preview tab, replacing the previous preview.
    void showInPreview(DashboardPage *page)
    {
        if (preview) {
            const int index = indexOf(preview);
            DashboardPage *old = preview;
            pages[std::size_t(index)] = page;
            preview = page;
            stack->removeWidget(old);
            delete old;
            tabBar->setTabText(index, pageTitle(page));
            tabBar->setTabIcon(index, pageIcon(page));
            syncPreview();
        } else {
            preview = page;
            insertTab(int(pages.size()), page);
        }
        setCurrent(page);
    }

    // Keeps the preview open: the next preview opens in a new tab.
    void pinPreview()
    {
        if (!preview)
            return;
        DashboardPage *page = preview;
        preview = nullptr;
        syncPreview();
        setCurrent(page);
    }

    // The page that shows one item alone. It is read-only and not attached to
    // the editor.
    DashboardPage *makeItemPage(const QString &taskId, const QString &itemId)
    {
        auto *page = new DashboardPage(services, DashboardRef{taskId, QString()}, stack, itemId);
        page->view->setSingleViewTracker(tracker);
        LayoutNode cell;
        cell.ref = CellRef{itemId, QString(), QString()};
        page->view->setTree(cell);
        page->title->setText(pageTitle(page));
        stack->addWidget(page);
        return page;
    }

    DashboardPage *itemPage(const QString &taskId, const QString &itemId) const
    {
        for (DashboardPage *page : pages) {
            if (page->itemId == itemId && page->ref.taskId == taskId)
                return page;
        }
        return nullptr;
    }

    // A task dashboard is named after its task and itself: "Task / Dashboard".
    QString taskDashboardTitle(const QString &taskId, const QString &dashboardId) const
    {
        const Task *task = model->task(taskId);
        if (!task)
            return QString();
        const Dashboard *dashboard = model->taskDashboard(taskId, dashboardId);
        return dashboard ? MainWindow::tr("%1 / %2").arg(task->title, dashboard->name) : task->title;
    }

    DashboardPage *makePage(const DashboardRef &ref)
    {
        auto *page = new DashboardPage(services, ref, stack);
        editor->attach(page->view, ref);
        page->view->setSingleViewTracker(tracker);
        if (const LayoutNode *tree = model->layout(ref))
            page->view->setTree(*tree);
        const bool unlocked = locks->isUnlocked(ref.dashboardId);
        page->view->setEditMode(unlocked);
        if (page->lock) {
            page->title->setText(title(ref));
            showLockState(page->lock, unlocked);
            QObject::connect(page->lock, &QToolButton::clicked, q, [this, id = ref.dashboardId] {
                locks->setUnlocked(id, !locks->isUnlocked(id));
            });
        }
        stack->addWidget(page);
        return page;
    }

    void insertTab(int index, DashboardPage *page)
    {
        pages.insert(pages.begin() + index, page);
        tabBar->insertTab(index, pageTitle(page));
        tabBar->setTabIcon(index, pageIcon(page));
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

    // Shows the page of the current tab, and its lock state next to the tabs.
    void syncCurrent()
    {
        DashboardPage *page = currentPage();
        if (page)
            stack->setCurrentWidget(page);
        updateLockButton();
        // When a task dashboard's tab comes to the front, the sidebar selects
        // that dashboard under its task, so it shows which task is on screen.
        // Only on a change of tab, so the user's own selection is kept.
        if (page != shownPage) {
            shownPage = page;
            if (page && !page->ref.isRoot() && sidebar)
                sidebar->reveal(page->itemId.isEmpty() ? page->ref.dashboardId : page->itemId);
        }
    }

    // The lock at the end of the tab bar row is a root dashboard's. A task
    // dashboard tab has its own lock in its title bar, so this one hides.
    void updateLockButton()
    {
        const DashboardPage *page = currentPage();
        lockButton->setVisible(!page || page->ref.isRoot());
        lockButton->setEnabled(page != nullptr);
        showLockState(lockButton, page && locks->isUnlocked(page->ref.dashboardId));
    }

    void applyLock(const QString &dashboardId, bool unlocked)
    {
        for (DashboardPage *page : pages) {
            if (page->ref.dashboardId != dashboardId)
                continue;
            page->view->setEditMode(unlocked);
            if (page->lock)
                showLockState(page->lock, unlocked);
        }
        updateLockButton();
    }

    // Where a root tab with this order goes: before the first open root tab
    // with a greater order, and always before the task dashboard tabs.
    int rootInsertIndex(int order) const
    {
        for (std::size_t i = 0; i < pages.size(); ++i) {
            const DashboardPage *page = pages[i];
            if (!page->ref.isRoot())
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

    // The open tab of a task dashboard, the preview or a pinned one, or nullptr.
    DashboardPage *taskPage(const DashboardRef &ref) const
    {
        for (DashboardPage *page : pages) {
            if (!page->ref.isRoot() && page->itemId.isEmpty() && page->ref == ref)
                return page;
        }
        return nullptr;
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

    // Adds a root dashboard with one cell of the item, placed before every
    // other root dashboard. Returns its id, or an empty string.
    QString addItemRootDashboard(const QString &taskId, const QString &itemId)
    {
        const Task *task = model->task(taskId);
        const Item *item = model->item(taskId, itemId);
        if (!task || !item)
            return QString();
        RootDashboard dashboard;
        dashboard.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString name = item->name.isEmpty() ? types->type(item->type).name() : item->name;
        dashboard.name = MainWindow::tr("%1 / %2").arg(task->title, name);
        dashboard.order = minOrder() - 1;
        dashboard.layout.ref = CellRef{itemId, taskId, QString()};
        model->addRootDashboard(dashboard);
        return dashboard.id;
    }

    // Adds a root dashboard with one cell of the task, placed before every
    // other root dashboard. Returns its id, or an empty string.
    QString addTaskRootDashboard(const QString &taskId, const QString &dashboardId)
    {
        const Task *task = model->task(taskId);
        if (!task)
            return QString();
        CellRef ref;
        ref.task = task->id;
        ref.dashboard = dashboardId;

        RootDashboard dashboard;
        dashboard.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        dashboard.name = taskDashboardTitle(task->id, dashboardId);
        dashboard.order = minOrder() - 1;
        dashboard.layout.ref = ref;
        model->addRootDashboard(dashboard);
        return dashboard.id;
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
        // An item page whose item is gone closes.
        for (int i = int(pages.size()) - 1; i >= 0; --i) {
            const DashboardPage *page = pages[std::size_t(i)];
            if (page->ref.taskId == taskId && !page->itemId.isEmpty() && !model->item(taskId, page->itemId))
                removeTab(i);
        }
        for (std::size_t i = 0; i < pages.size(); ++i) {
            DashboardPage *page = pages[i];
            if (page->ref.taskId != taskId)
                continue;
            if (page->itemId.isEmpty()) {
                if (const LayoutNode *tree = model->layout(page->ref))
                    page->view->setTree(*tree);
            }
            tabBar->setTabText(int(i), pageTitle(page));
            if (page->title)
                page->title->setText(pageTitle(page));
        }
        tabBar->updateGeometry();
        syncCurrent();

        // Editing the dashboard in the preview tab pins it (docs/ui.md). The
        // change may come from the preview's own view, so pin after it returns.
        if (preview && preview->itemId.isEmpty() && preview->ref.taskId == taskId && currentPage() == preview) {
            const QString dashboardId = preview->ref.dashboardId;
            QTimer::singleShot(0, q, [window = q, taskId, dashboardId] { window->pinTask(taskId, dashboardId); });
        }
    }

    QString workspace;              // the open data folder; empty for the sample data
    QAction *showSidebarAction = nullptr;

    static constexpr int maxRecent = 10;

    // The workspace's name, else its folder's name, else "Sample data".
    QString workspaceDisplayName() const
    {
        if (!model->workspaceName().isEmpty())
            return model->workspaceName();
        return workspace.isEmpty() ? MainWindow::tr("Sample data") : QDir(workspace).dirName();
    }

    void updateTitle()
    {
        const QString name = workspaceDisplayName();
        q->setWindowTitle(MainWindow::tr("%1 - %2").arg(name, QStringLiteral(MIRANTE_APP_NAME)));
        if (sidebar)
            sidebar->setTitle(name);
    }

    // Asks for a new name for the open workspace.
    void promptRename()
    {
        if (workspace.isEmpty()) {
            QMessageBox::information(q, MainWindow::tr("Rename Workspace"),
                                     MainWindow::tr("The sample data cannot be renamed. Open a workspace folder first."));
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(q, MainWindow::tr("Rename Workspace"), MainWindow::tr("Name:"),
                                                   QLineEdit::Normal, workspaceDisplayName(), &ok)
                                 .trimmed();
        if (ok && !name.isEmpty())
            q->renameWorkspace(name);
    }

    QStringList recent() const
    {
        return QSettings().value(QStringLiteral("workspace/recent")).toStringList();
    }

    void remember(const QString &folder)
    {
        QSettings settings;
        settings.setValue(QStringLiteral("workspace/last"), folder);
        if (folder.isEmpty())
            return;
        QStringList list = recent();
        list.removeAll(folder);
        list.prepend(folder);
        while (list.size() > maxRecent)
            list.removeLast();
        settings.setValue(QStringLiteral("workspace/recent"), list);
    }

    void forget(const QString &folder)
    {
        QStringList list = recent();
        list.removeAll(folder);
        QSettings().setValue(QStringLiteral("workspace/recent"), list);
    }

    // The File, View and Help menus.
    void buildMenus()
    {
        QMenuBar *bar = q->menuBar();

        QMenu *file = bar->addMenu(MainWindow::tr("&File"));
        QAction *open = file->addAction(MainWindow::tr("&Open Workspace…"));
        open->setShortcut(QKeySequence::Open);
        QObject::connect(open, &QAction::triggered, q, [this] {
            const QString folder = QFileDialog::getExistingDirectory(q, MainWindow::tr("Open Workspace"), workspace);
            if (!folder.isEmpty())
                q->openWorkspace(folder);
        });
        QMenu *recentMenu = file->addMenu(MainWindow::tr("Open &Recent"));
        QObject::connect(recentMenu, &QMenu::aboutToShow, q, [this, recentMenu] {
            recentMenu->clear();
            const QStringList folders = recent();
            for (const QString &folder : folders) {
                QAction *action = recentMenu->addAction(QDir::toNativeSeparators(folder));
                QObject::connect(action, &QAction::triggered, q, [this, folder] { q->openWorkspace(folder); });
            }
            if (folders.isEmpty())
                recentMenu->addAction(MainWindow::tr("No recent workspaces"))->setEnabled(false);
            recentMenu->addSeparator();
            QAction *clear = recentMenu->addAction(MainWindow::tr("Clear Recent"));
            clear->setEnabled(!folders.isEmpty());
            QObject::connect(clear, &QAction::triggered, q,
                             [] { QSettings().remove(QStringLiteral("workspace/recent")); });
        });
        QAction *rename = file->addAction(MainWindow::tr("Re&name Workspace…"));
        QObject::connect(rename, &QAction::triggered, q, [this] { promptRename(); });
        QObject::connect(file, &QMenu::aboutToShow, q, [this, rename] { rename->setEnabled(!workspace.isEmpty()); });
        QAction *sample = file->addAction(MainWindow::tr("Open &Sample Data"));
        QObject::connect(sample, &QAction::triggered, q, &MainWindow::openSampleData);
        QAction *reload = file->addAction(MainWindow::tr("Re&load Workspace"));
        reload->setShortcut(QKeySequence::Refresh);
        QObject::connect(reload, &QAction::triggered, q, [this] {
            if (workspace.isEmpty())
                q->openSampleData();
            else
                q->openWorkspace(workspace);
        });
        file->addSeparator();
        QAction *quit = file->addAction(MainWindow::tr("&Quit"));
        quit->setShortcut(QKeySequence::Quit);
        quit->setMenuRole(QAction::QuitRole);
        QObject::connect(quit, &QAction::triggered, q, &QWidget::close);

        QMenu *view = bar->addMenu(MainWindow::tr("&View"));
        showSidebarAction = view->addAction(MainWindow::tr("&Sidebar"));
        showSidebarAction->setCheckable(true);
        showSidebarAction->setChecked(true);
        QObject::connect(showSidebarAction, &QAction::triggered, q, [this](bool on) { showSidebar(on); });
        QAction *expand = view->addAction(MainWindow::tr("&Expand All Tasks"));
        QObject::connect(expand, &QAction::triggered, sidebar, &Sidebar::expandAllTasks);
        QAction *collapse = view->addAction(MainWindow::tr("&Collapse All Tasks"));
        QObject::connect(collapse, &QAction::triggered, sidebar, &Sidebar::collapseAllTasks);
        view->addSeparator();
        QAction *switcherAction = view->addAction(MainWindow::tr("&Go To…"));
        switcherAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
        QObject::connect(switcherAction, &QAction::triggered, q, &MainWindow::showQuickSwitcher);
        QAction *list = view->addAction(MainWindow::tr("&All Dashboards…"));
        QObject::connect(list, &QAction::triggered, q, &MainWindow::showDashboardList);
        QAction *add = view->addAction(MainWindow::tr("&New Dashboard"));
        QObject::connect(add, &QAction::triggered, q, &MainWindow::newRootDashboard);

        QMenu *help = bar->addMenu(MainWindow::tr("&Help"));
        QAction *about = help->addAction(MainWindow::tr("&About Mirante"));
        about->setMenuRole(QAction::AboutRole);
        QObject::connect(about, &QAction::triggered, q, &MainWindow::showAbout);
        QAction *aboutQt = help->addAction(MainWindow::tr("About &Qt"));
        aboutQt->setMenuRole(QAction::AboutQtRole);
        QObject::connect(aboutQt, &QAction::triggered, q, [] { QApplication::aboutQt(); });
    }

    void showSidebar(bool show)
    {
        if (showSidebarAction)
            showSidebarAction->setChecked(show);
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

        // The button that brings back a hidden sidebar, before the tabs, and
        // the current dashboard's lock, after the + button.
        showSidebarButton = new QToolButton(content);
        showSidebarButton->setObjectName(QStringLiteral("showSidebarButton"));
        showSidebarButton->setText(QStringLiteral("»"));
        showSidebarButton->setToolTip(tr("Show sidebar"));
        showSidebarButton->setAutoRaise(true);
        showSidebarButton->hide();

        lockButton = new QToolButton(content);
        lockButton->setObjectName(QStringLiteral("lockButton"));
        lockButton->setAutoRaise(true);

        listButton = new QToolButton(content);
        listButton->setObjectName(QStringLiteral("dashboardListButton"));
        listButton->setIcon(QIcon(QStringLiteral(":/icons/dashboards.svg")));
        listButton->setText(tr("All dashboards"));
        listButton->setToolTip(tr("All root dashboards, including closed ones"));
        listButton->setAutoRaise(true);

        tabRow->addWidget(showSidebarButton);
        tabRow->addWidget(tabBar);
        tabRow->addWidget(addButton);
        tabRow->addWidget(listButton);
        tabRow->addStretch(1);
        tabRow->addWidget(lockButton);      // at the far right

        stack = new QStackedWidget(content);

        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(buildWarningBar());
        layout->addLayout(tabRow);
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
    d->services.openTaskAlone = [this](const QString &taskId, const QString &dashboardId) {
        openTaskPreview(taskId, dashboardId);
    };

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

    // The window's size and sidebar width from the last session on this
    // device. The window always opens centred on the screen.
    const QSettings settings;
    const QSize size = settings.value(QStringLiteral("window/size")).toSize();
    if (size.isValid())
        resize(size);
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect frame(QPoint(0, 0), this->size());
        frame.moveCenter(screen->availableGeometry().center());
        move(frame.topLeft());
    }
    if (settings.value(QStringLiteral("window/maximized")).toBool())
        setWindowState(windowState() | Qt::WindowMaximized);
    d->splitter->restoreState(settings.value(QStringLiteral("window/splitter")).toByteArray());
    setCentralWidget(d->splitter);

    // The sidebar.
    connect(d->sidebar, &Sidebar::dashboardOpened, this, &MainWindow::openTaskPreview);
    connect(d->sidebar, &Sidebar::itemOpened, this, &MainWindow::openItem);
    connect(d->sidebar, &Sidebar::openInNewRootDashboard, this, [this](const QString &taskId, const QString &dashboardId) {
        const QString id = d->addTaskRootDashboard(taskId, dashboardId);
        if (!id.isEmpty())
            openRootDashboard(id);
    });
    connect(d->sidebar, &Sidebar::openItemInNewRootDashboard, this, [this](const QString &taskId, const QString &itemId) {
        const QString id = d->addItemRootDashboard(taskId, itemId);
        if (!id.isEmpty())
            openRootDashboard(id);
    });
    connect(d->sidebar, &Sidebar::collapseRequested, this, [this] { d->showSidebar(false); });
    connect(d->sidebar, &Sidebar::titleDoubleClicked, this, [this] { d->promptRename(); });
    connect(&model, &Model::workspaceNameChanged, this, [this] { d->updateTitle(); });
    connect(d->showSidebarButton, &QToolButton::clicked, this, [this] { d->showSidebar(true); });

    // The tab bar.
    connect(d->tabBar, &QTabBar::currentChanged, this, [this] { d->syncCurrent(); });
    connect(d->tabBar, &QTabBar::tabCloseRequested, this, [this](int index) { d->removeTab(index); });
    connect(d->tabBar, &RootTabBar::pinRequested, this, [this](int index) {
        if (index >= 0 && index < int(d->pages.size()) && d->pages[std::size_t(index)] == d->preview)
            d->pinPreview();
    });
    connect(d->tabBar, &RootTabBar::newDashboardRequested, this, &MainWindow::newRootDashboard);
    connect(d->tabBar, &RootTabBar::dashboardListRequested, this, &MainWindow::showDashboardList);
    connect(d->listButton, &QToolButton::clicked, this, &MainWindow::showDashboardList);

    // Locks.
    connect(d->lockButton, &QToolButton::clicked, this, [this] {
        if (const DashboardPage *page = d->currentPage())
            d->locks->setUnlocked(page->ref.dashboardId, !d->locks->isUnlocked(page->ref.dashboardId));
    });
    connect(d->locks, &LockState::changed, this,
            [this](const QString &id, bool unlocked) { d->applyLock(id, unlocked); });

    // The model.
    connect(&model, &Model::rootDashboardChanged, this, [this](const QString &id) { d->onRootDashboardChanged(id); });
    // A deleted root dashboard loses its tab. Closing a tab does not delete it.
    connect(&model, &Model::rootDashboardRemoved, this, [this](const QString &id) {
        if (DashboardPage *page = d->rootPage(id))
            d->removeTab(d->indexOf(page));
    });
    connect(&model, &Model::taskChanged, this, [this](const QString &taskId) { d->onTaskChanged(taskId); });
    connect(&model, &Model::modelReset, this, [this] {
        d->closeAllTabs();
        d->openInitialTabs();
    });

    // The switcher and the shortcuts.
    d->switcher = new QuickSwitcher(model, this);
    connect(d->switcher, &QuickSwitcher::activated, this,
            [this](const SwitcherEntry &entry) { d->onSwitcherActivated(entry); });
    d->buildMenus();

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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // The size of the normal, not maximized, window is kept.
    QSettings settings;
    settings.setValue(QStringLiteral("window/size"), normalGeometry().size());
    settings.setValue(QStringLiteral("window/maximized"), isMaximized());
    settings.setValue(QStringLiteral("window/splitter"), d->splitter->saveState());
    QMainWindow::closeEvent(event);
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

void MainWindow::openWorkspace(const QString &folder)
{
    if (folder.isEmpty() || !QDir(folder).exists()) {
        d->forget(folder);
        setLoadProblems({LoadProblem{QDir::toNativeSeparators(folder), tr("The workspace folder does not exist.")}});
        return;
    }
    const std::vector<LoadProblem> problems = store::load(folder, *d->model);
    d->workspace = QDir(folder).absolutePath();
    d->remember(d->workspace);
    d->updateTitle();
    setLoadProblems(problems);
}

void MainWindow::openSampleData()
{
    const std::vector<LoadProblem> problems = store::load(QStringLiteral(":/fixtures"), *d->model);
    d->workspace.clear();
    d->remember(QString());
    d->updateTitle();
    setLoadProblems(problems);
}

bool MainWindow::renameWorkspace(const QString &name)
{
    if (d->workspace.isEmpty() || name.trimmed().isEmpty())
        return false;
    QString error;
    if (!store::saveWorkspaceName(d->workspace, name.trimmed(), &error)) {
        setLoadProblems({LoadProblem{QStringLiteral("workspace.json"), tr("cannot be written: %1").arg(error)}});
        return false;
    }
    d->model->setWorkspaceName(name.trimmed());
    return true;
}

QString MainWindow::workspace() const { return d->workspace; }

QStringList MainWindow::recentWorkspaces() const { return d->recent(); }

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About %1").arg(QStringLiteral(MIRANTE_APP_NAME)),
        tr("<h3>%1 %2</h3><p>%3</p><p>%4 &lt;<a href=\"mailto:%5\">%5</a>&gt;</p>"
           "<p><a href=\"%6\">%6</a></p><p>Released under the MIT License.</p>")
            .arg(QStringLiteral(MIRANTE_APP_NAME), QStringLiteral(MIRANTE_VERSION),
                 QStringLiteral(MIRANTE_DESCRIPTION), QStringLiteral(MIRANTE_AUTHOR),
                 QStringLiteral(MIRANTE_AUTHOR_EMAIL), QStringLiteral(MIRANTE_HOMEPAGE)));
}

void MainWindow::openTaskPreview(const QString &taskId, const QString &chosenDashboardId)
{
    const Task *task = d->model->task(taskId);
    if (!task)
        return;
    const QString dashboardId = chosenDashboardId.isEmpty() ? Private::defaultDashboardOf(*task) : chosenDashboardId;
    if (dashboardId.isEmpty() || !d->model->taskDashboard(taskId, dashboardId))
        return;

    // A task dashboard already open in a tab, preview or pinned, is shown there.
    const DashboardRef ref{taskId, dashboardId};
    if (DashboardPage *open = d->taskPage(ref)) {
        d->setCurrent(open);
        return;
    }

    d->showInPreview(d->makePage(ref));
}

void MainWindow::pinTask(const QString &taskId, const QString &chosenDashboardId)
{
    const Task *task = d->model->task(taskId);
    if (!task)
        return;
    const QString dashboardId = chosenDashboardId.isEmpty() ? Private::defaultDashboardOf(*task) : chosenDashboardId;
    const DashboardRef ref{taskId, dashboardId};
    if (dashboardId.isEmpty() || !d->model->taskDashboard(taskId, dashboardId))
        return;

    // Pinning keeps the task dashboard's tab open: the preview stops being the
    // preview, so the next one opens in a new tab. It is not a root dashboard.
    if (d->preview && d->preview->itemId.isEmpty() && d->preview->ref == ref) {
        d->pinPreview();
        return;
    }
    DashboardPage *page = d->taskPage(ref);
    if (!page) {
        page = d->makePage(ref);
        d->insertTab(d->preview ? d->indexOf(d->preview) : int(d->pages.size()), page);
    }
    d->setCurrent(page);
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

void MainWindow::showDashboardList()
{
    if (!d->dashboardList) {
        d->dashboardList = new RootDashboardsDialog(
            *d->model, [this](const QString &id) { return d->rootPage(id) != nullptr; }, this);
        d->dashboardList->setAttribute(Qt::WA_DeleteOnClose);
        connect(d->dashboardList, &RootDashboardsDialog::openRequested, this, &MainWindow::openRootDashboard);
    }
    d->dashboardList->show();
    d->dashboardList->raise();
    d->dashboardList->activateWindow();
}

void MainWindow::openItem(const QString &taskId, const QString &itemId)
{
    if (!d->model->item(taskId, itemId))
        return;
    if (DashboardPage *open = d->itemPage(taskId, itemId)) {
        d->setCurrent(open);
        return;
    }
    d->showInPreview(d->makeItemPage(taskId, itemId));
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
