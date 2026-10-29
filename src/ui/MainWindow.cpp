#include "ui/MainWindow.h"

#include "Version.h"
#include "core/DeviceSettings.h"
#include "core/LayoutTree.h"
#include "core/LogBuffer.h"
#include "core/Model.h"
#include "core/Sorting.h"
#include "items/ItemTypeRegistry.h"
#include "items/Placement.h"
#include "layout/CellFrame.h"
#include "layout/CellHoverTracker.h"
#include "layout/CellStyle.h"
#include "layout/DashboardCellFactory.h"
#include "layout/HeaderPill.h"
#include "layout/LayoutEditor.h"
#include "layout/LayoutServices.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"
#include "layout/SingleViewTracker.h"
#include "layout/TaskCell.h"
#include "ui/ConflictBar.h"
#include "ui/DashboardDialog.h"
#include "ui/DashboardPage.h"
#include "ui/DetachedWindow.h"
#include "ui/EntityActions.h"
#include "ui/FocusView.h"
#include "ui/LogWindow.h"
#include "ui/ImagePaste.h"
#include "ui/QuickSwitcher.h"
#include "ui/RootDashboardsDialog.h"
#include "ui/RootTabBar.h"
#include "ui/Shortcuts.h"
#include "ui/Sidebar.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QModelIndex>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

#include <algorithm>
#include <limits>

namespace mirante {

namespace {

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
    Store *store = nullptr;
    DeviceSettings *settings = nullptr;
    ProfileManager *profiles = nullptr;     // null in tests without web views
    ViewPool *pool = nullptr;
    EntityActions *actions = nullptr;
    std::unique_ptr<ImagePaste> imagePaste;
    std::vector<LoadProblem> ownProblems;   // set with setLoadProblems(), such as a missing folder

    LayoutEditor *editor = nullptr;
    LockState *locks = nullptr;
    SingleViewTracker *tracker = nullptr;
    LayoutServices services;
    PlacementServices placements;           // services.placements points here

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
    QPointer<LogWindow> logWindow;

    std::vector<DashboardPage *> pages;     // in tab order
    DashboardPage *preview = nullptr;       // also in pages, always last
    // The windows of detached root dashboards, in the order they were
    // detached. Their pages are not in pages. See docs/ui.md#detached-windows.
    std::vector<DetachedWindow *> detached;

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
        if (page->isFocus())
            return MainWindow::tr("Focus");
        if (page->itemId.isEmpty())
            return title(page->ref);
        const Task *task = model->task(page->ref.taskId);
        const Item *item = model->item(page->ref.taskId, page->itemId);
        if (!task || !item)
            return QString();
        const QString name = item->name.isEmpty() ? types->type(item->type).name() : item->name;
        return MainWindow::tr("%1 / %2").arg(task->title, name);
    }

    // The project of a task dashboard or item page, or empty.
    QString pageProject(const DashboardPage *page) const
    {
        const Task *task = page->ref.isRoot() ? nullptr : model->task(page->ref.taskId);
        const Project *project = task ? model->project(task->projectId) : nullptr;
        return project ? project->name : QString();
    }

    // A tab's tooltip names the project too, which its title leaves out.
    QString pageToolTip(const DashboardPage *page) const
    {
        const QString project = pageProject(page);
        return project.isEmpty() ? pageTitle(page) : MainWindow::tr("%1 / %2").arg(project, pageTitle(page));
    }

    // The header of a task dashboard or item page, in pills: the task, then
    // the dashboard with its icon or the item with its type's icon and
    // colour, then the project in a grey pill.
    void updatePageHeader(DashboardPage *page) const
    {
        if (!page->title)
            return;
        const Task *task = model->task(page->ref.taskId);
        page->title->setText(task ? task->title : QString());
        QString part;
        if (page->itemId.isEmpty()) {
            const Dashboard *dashboard = model->taskDashboard(page->ref.taskId, page->ref.dashboardId);
            part = dashboard ? dashboard->name : QString();
            page->part->setFill(HeaderPill::Fill::Neutral);
            page->part->setIcon(QIcon(QStringLiteral(":/icons/dashboard.svg")));
        } else if (const Item *item = model->item(page->ref.taskId, page->itemId)) {
            const ItemType &type = types->type(item->type);
            part = item->name.isEmpty() ? type.name() : item->name;
            if (types->contains(item->type))
                page->part->setFill(HeaderPill::Fill::Module, item->type.section(QLatin1Char('.'), 0, 0));
            else
                page->part->setFill(HeaderPill::Fill::Neutral);
            page->part->setIcon(type.icon());
        }
        page->part->setText(part);
        page->part->setToolTip(part);
        page->title->setToolTip(page->title->text());
        page->context->setText(pageProject(page));
        page->headerRow->refresh();
    }

    // Task dashboard tabs carry the dashboard icon and item tabs the item's
    // type icon, so they read apart from root dashboard tabs, which have none.
    // Focus is a list, not a dashboard, so its tab has an icon of its own.
    QIcon pageIcon(const DashboardPage *page) const
    {
        if (page->isFocus())
            return QIcon(QStringLiteral(":/icons/focus.svg"));
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
            tabBar->setTabToolTip(index, pageToolTip(page));
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
        QObject::connect(page->view, &LayoutView::cellMenuRequested, q,
                         [this, view = page->view, taskId, itemId](const NodePath &, const QPoint &globalPos) {
                             editor->showItemMenu(view, taskId, itemId, globalPos);
                         });
        LayoutNode cell;
        cell.ref = CellRef{itemId, QString(), QString()};
        page->view->setTree(cell);
        updatePageHeader(page);
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
        if (const LayoutNode *tree = model->layout(ref)) {
            page->view->setTree(*tree);
            page->shownLayout = layoutToJson(*tree);
        }
        const bool unlocked = locks->isUnlocked(ref.dashboardId);
        page->view->setEditMode(unlocked);
        if (page->lock) {
            updatePageHeader(page);
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
        tabBar->setTabToolTip(index, pageToolTip(page));
        // Only root dashboard tabs can be detached (decision 0024).
        tabBar->setTabData(index, page->isRoot());
        syncPreview();
        syncCurrent();
    }

    // Takes a page out of the tab bar without deleting it.
    DashboardPage *takeTab(DashboardPage *page)
    {
        const int index = indexOf(page);
        if (index < 0)
            return page;
        pages.erase(pages.begin() + index);
        if (preview == page)
            preview = nullptr;
        stack->removeWidget(page);
        tabBar->removeTab(index);
        syncPreview();
        syncCurrent();
        return page;
    }

    void removeTab(int index)
    {
        if (index >= 0 && index < int(pages.size()))
            delete takeTab(pages[std::size_t(index)]);
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
        lockButton->setEnabled(page && page->isRoot());
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

    DetachedWindow *detachedWindow(const QString &dashboardId) const
    {
        for (DetachedWindow *window : detached) {
            if (window->dashboardId() == dashboardId)
                return window;
        }
        return nullptr;
    }

    QStringList detachedIds() const
    {
        QStringList ids;
        for (const DetachedWindow *window : detached)
            ids << window->dashboardId();
        return ids;
    }

    // Moves the page of a root dashboard into a new window of its own. The
    // page is moved, not made again, so its web pages keep their state. Only
    // the cells that cannot move are made again.
    DetachedWindow *openInWindow(DashboardPage *page)
    {
        stack->removeWidget(page);
        auto *window = new DetachedWindow(page, locks, *settings);
        window->setTitle(title(page->ref));
        QObject::connect(window, &DetachedWindow::putBackRequested, q, [this](const QString &id) { putBack(id); });
        detached.push_back(window);
        window->show();
        page->view->remakeUnsafeCells();
        return window;
    }

    // "Detach into Window" on a root dashboard tab: the tab leaves the tab bar
    // and its page moves into a new window.
    // Renames the root dashboard of a tab. The tab title follows the model.
    void renameRoot(int index)
    {
        if (index < 0 || index >= int(pages.size()) || !pages[std::size_t(index)]->isRoot())
            return;
        const RootDashboard *dashboard = model->rootDashboard(pages[std::size_t(index)]->ref.dashboardId);
        if (!dashboard)
            return;
        DashboardDialog dialog(*dashboard, false, q);
        if (dialog.exec() != QDialog::Accepted)
            return;
        RootDashboard changed = *dashboard;
        changed.name = dialog.value().name;
        model->setRootDashboard(changed);
    }

    void detach(int index)
    {
        if (index < 0 || index >= int(pages.size()) || !pages[std::size_t(index)]->isRoot())
            return;
        DetachedWindow *window = openInWindow(takeTab(pages[std::size_t(index)]));
        window->raise();
        window->activateWindow();
    }

    // The window was closed or "Put Back" was clicked: its page goes back to
    // the end of the tab bar, before the preview tab, and becomes current.
    void putBack(const QString &dashboardId)
    {
        DetachedWindow *window = detachedWindow(dashboardId);
        if (!window)
            return;
        detached.erase(std::find(detached.begin(), detached.end(), window));
        window->disconnect(q);
        DashboardPage *page = window->takePage();
        // This may run inside the window's own close event.
        window->hide();
        window->deleteLater();
        if (!page)
            return;
        stack->addWidget(page);
        insertTab(preview ? indexOf(preview) : int(pages.size()), page);
        page->view->remakeUnsafeCells();
        setCurrent(page);
    }

    // Puts every detached root dashboard back in the tab bar.
    void putBackAll()
    {
        const std::vector<DetachedWindow *> windows = detached;
        for (DetachedWindow *window : windows)
            window->close();    // stores its geometry, then asks to be put back
    }

    // Deletes a detached window with its page.
    void deleteWindow(DetachedWindow *window)
    {
        detached.erase(std::find(detached.begin(), detached.end(), window));
        window->disconnect(q);
        delete window;
    }

    // Closes the detached windows without putting them back, as when another
    // workspace opens. Each stores its geometry first.
    void closeDetachedWindows()
    {
        while (!detached.empty()) {
            DetachedWindow *window = detached.back();
            window->disconnect(q);
            window->close();
            deleteWindow(window);
        }
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
        if (DetachedWindow *window = detachedWindow(id)) {
            if (const LayoutNode *tree = model->layout(window->page()->ref))
                window->page()->view->setTree(*tree);
            window->setTitle(title(window->page()->ref));
        }
        tabBar->updateGeometry();
        syncCurrent();
    }

    void onTaskChanged(const QString &taskId)
    {
        // A task moved to the trash loses its pages, as a removed one does.
        if (model->taskInTrash(taskId)) {
            onTaskRemoved(taskId);
            return;
        }
        // An item page whose item is gone closes, and so does a task dashboard
        // page whose dashboard is gone.
        for (int i = int(pages.size()) - 1; i >= 0; --i) {
            const DashboardPage *page = pages[std::size_t(i)];
            if (page->ref.taskId != taskId)
                continue;
            const bool gone = page->itemId.isEmpty() ? !model->taskDashboard(taskId, page->ref.dashboardId)
                                                     : !model->item(taskId, page->itemId);
            if (gone)
                removeTab(i);
        }
        bool previewEdited = false;
        for (std::size_t i = 0; i < pages.size(); ++i) {
            DashboardPage *page = pages[i];
            if (page->ref.taskId != taskId)
                continue;
            if (page->itemId.isEmpty()) {
                if (const LayoutNode *tree = model->layout(page->ref)) {
                    page->view->setTree(*tree);
                    QJsonObject shown = layoutToJson(*tree);
                    if (page == preview && shown != page->shownLayout)
                        previewEdited = true;
                    page->shownLayout = std::move(shown);
                }
            }
            tabBar->setTabText(int(i), pageTitle(page));
            tabBar->setTabToolTip(int(i), pageToolTip(page));
            updatePageHeader(page);
        }
        tabBar->updateGeometry();
        syncCurrent();

        // Editing the dashboard in the preview tab pins it (docs/ui.md). Other
        // changes to its task, such as a new dashboard, do not. The change may
        // come from the preview's own view, so pin after it returns.
        if (previewEdited && currentPage() == preview) {
            const QString dashboardId = preview->ref.dashboardId;
            QTimer::singleShot(0, q, [window = q, taskId, dashboardId] { window->pinTask(taskId, dashboardId); });
        }
    }

    // The pages of a removed task close.
    void onTaskRemoved(const QString &taskId)
    {
        for (int i = int(pages.size()) - 1; i >= 0; --i) {
            if (pages[std::size_t(i)]->ref.taskId == taskId)
                removeTab(i);
        }
    }

    // Closes the pages of every task in the trash, such as the tasks of a
    // project that just moved there.
    void closeTrashedPages()
    {
        for (int i = int(pages.size()) - 1; i >= 0; --i) {
            const QString &taskId = pages[std::size_t(i)]->ref.taskId;
            if (!taskId.isEmpty() && model->taskInTrash(taskId))
                removeTab(i);
        }
    }

    DashboardPage *focusPage() const
    {
        for (DashboardPage *page : pages) {
            if (page->isFocus())
                return page;
        }
        return nullptr;
    }

    // The Focus page, at the start of the tab bar.
    DashboardPage *makeFocusPage()
    {
        auto *view = new FocusView(*model);
        QObject::connect(view, &FocusView::taskOpened, q, [this](const QString &taskId) { q->openTaskPreview(taskId); });
        view->setMenuFiller([this](QMenu *menu, const QString &taskId) {
            actions->fillMenu(menu, SidebarModel::Kind::Task, taskId, taskId);
        });
        auto *page = new DashboardPage(view, stack);
        stack->addWidget(page);
        return page;
    }

    // The open tabs, as DeviceSettings keeps them: root dashboards and Focus in
    // tab order, the current one, and the detached root dashboards. The preview
    // tab, pinned task tabs and item tabs are not kept.
    DeviceSettings::Tabs openTabs() const
    {
        DeviceSettings::Tabs tabs;
        tabs.saved = true;
        for (const DashboardPage *page : pages) {
            if (page->isFocus())
                tabs.ids << focusTabId;
            else if (page->isRoot())
                tabs.ids << page->ref.dashboardId;
        }
        // Another current tab, which is not kept, falls back to the first.
        const DashboardPage *current = currentPage();
        if (current && current->isFocus())
            tabs.current = focusTabId;
        else if (current && current->isRoot())
            tabs.current = current->ref.dashboardId;
        else if (!tabs.ids.isEmpty())
            tabs.current = tabs.ids.front();
        tabs.detached = detachedIds();
        return tabs;
    }

    // Replaces the open tabs and detached windows with saved ones. Ids that
    // no longer exist are skipped. A detached window opens at its stored
    // geometry.
    void restoreTabs(const DeviceSettings::Tabs &tabs)
    {
        closeDetachedWindows();
        closeAllTabs();
        for (const QString &id : tabs.ids) {
            if (id == focusTabId) {
                if (!focusPage())
                    insertTab(int(pages.size()), makeFocusPage());
            } else if (model->rootDashboard(id) && !rootPage(id)) {
                insertTab(int(pages.size()), makePage(DashboardRef{QString(), id}));
            }
        }
        for (const QString &id : tabs.detached) {
            if (model->rootDashboard(id) && !rootPage(id) && !detachedWindow(id))
                openInWindow(makePage(DashboardRef{QString(), id}));
        }
        const DashboardPage *current = nullptr;
        if (tabs.current == focusTabId)
            current = focusPage();
        else if (!tabs.current.isEmpty())
            current = rootPage(tabs.current);
        if (!current && !pages.empty())
            current = pages.front();
        if (current)
            setCurrent(current);
    }

    // The key the open tabs are saved under: the Store's folder, or none
    // before anything was opened through it.
    QString tabsKey() const { return store->dir(); }

    void saveOpenTabs()
    {
        if (!tabsKey().isEmpty())
            settings->setOpenTabs(tabsKey(), openTabs());
    }

    // After opening a workspace: its saved tabs, or else every root dashboard,
    // which the model's reset already opened.
    void restoreOpenTabs()
    {
        const DeviceSettings::Tabs tabs = settings->openTabs(tabsKey());
        if (tabs.saved)
            restoreTabs(tabs);
    }

    // The warning bar: the read-only note of the sample data, the problems
    // set by the window, and the Store's problems.
    void updateWarnings()
    {
        std::vector<LoadProblem> problems = ownProblems;
        const std::vector<LoadProblem> stored = store->problems();
        problems.insert(problems.end(), stored.begin(), stored.end());
        const bool readOnly = store->readOnly() && !store->dir().isEmpty();

        QStringList lines;
        if (readOnly)
            lines << MainWindow::tr("Changes to the sample data are not saved. "
                                    "Use File > New Workspace… to keep your own data.");
        if (!problems.empty()) {
            lines << MainWindow::tr("Some files could not be loaded or saved:");
            for (const LoadProblem &problem : problems)
                lines << MainWindow::tr("%1: %2").arg(problem.file, problem.message);
        }
        if (lines.isEmpty()) {
            warningText->clear();
            warningBar->hide();
            return;
        }
        warningText->setText(lines.join(QLatin1Char('\n')));
        warningBar->show();
    }

    // Opens a folder, or the sample data, through the Store, keeping the open
    // tabs of the one before.
    void openThroughStore(const QString &dir)
    {
        // The detached windows close after their state is saved. The saved
        // tabs of the next workspace bring back its own.
        saveOpenTabs();
        closeDetachedWindows();
        ownProblems.clear();
        store->open(dir);
        restoreOpenTabs();
        updateWarnings();
    }

    // Asks which task an image pasted onto a root dashboard belongs to. It
    // offers the active tasks in sidebar order, with the task of the sidebar's
    // selection chosen. Returns the task's id, an empty id if there is no
    // active task, or nullopt if the user cancels.
    std::optional<QString> askTaskForImage()
    {
        QStringList labels;
        QStringList ids;
        const SortFilter activeTasks;
        for (const Group *group : model->groups()) {
            for (const Project *project : sortedProjects(model->projectsOfGroup(group->id), activeTasks)) {
                for (const Task *task : sortedTasks(model->tasksOfProject(project->id), activeTasks)) {
                    labels.append(MainWindow::tr("%1 / %2").arg(project->name, task->title));
                    ids.append(task->id);
                }
            }
        }
        if (ids.isEmpty())
            return QString();
        const QString selected = sidebar->currentIndex().data(SidebarModel::TaskIdRole).toString();
        QInputDialog dialog(q);
        dialog.setWindowTitle(MainWindow::tr("Paste Image"));
        dialog.setLabelText(MainWindow::tr("Add the image to the task:"));
        dialog.setComboBoxItems(labels);
        dialog.setComboBoxEditable(false);
        dialog.setTextValue(labels.at(std::max(0, int(ids.indexOf(selected)))));
        if (dialog.exec() != QDialog::Accepted)
            return std::nullopt;
        // The combo box's index, since two tasks can share a label.
        const auto *combo = dialog.findChild<QComboBox *>();
        const int index = combo ? combo->currentIndex() : int(labels.indexOf(dialog.textValue()));
        return ids.value(index);
    }

    // The group and project the sidebar's selection belongs to, for the +
    // menu. Each is empty if the selection has none.
    std::pair<QString, QString> selectedGroupAndProject() const
    {
        const QModelIndex index = sidebar->currentIndex();
        if (!index.isValid())
            return {};
        const auto kind = SidebarModel::Kind(index.data(SidebarModel::KindRole).toInt());
        const QString id = index.data(SidebarModel::IdRole).toString();
        QString projectId;
        switch (kind) {
        case SidebarModel::Kind::Group:
            return {id, QString()};
        case SidebarModel::Kind::Project:
            projectId = id;
            break;
        default:
            if (const Task *task = model->task(index.data(SidebarModel::TaskIdRole).toString()))
                projectId = task->projectId;
            break;
        }
        const Project *project = model->project(projectId);
        return project ? std::make_pair(project->groupId, project->id) : std::pair<QString, QString>();
    }

    // The + menu in the sidebar header: New Group, New Project in the selected
    // group, New Task in the selected project.
    QToolButton *buildNewButton()
    {
        auto *button = new QToolButton;
        button->setObjectName(QStringLiteral("newEntityButton"));
        button->setText(QStringLiteral("+"));
        button->setToolTip(MainWindow::tr("New group, project or task"));
        button->setAutoRaise(true);
        button->setPopupMode(QToolButton::InstantPopup);
        auto *menu = new QMenu(button);
        QAction *group = menu->addAction(MainWindow::tr("New &Group…"));
        QAction *project = menu->addAction(MainWindow::tr("New &Project…"));
        QAction *task = menu->addAction(MainWindow::tr("New &Task…"));
        QObject::connect(menu, &QMenu::aboutToShow, q, [this, project, task] {
            const auto [groupId, projectId] = selectedGroupAndProject();
            project->setEnabled(!groupId.isEmpty());
            task->setEnabled(!projectId.isEmpty());
        });
        QObject::connect(group, &QAction::triggered, q, [this] { revealNew(actions->newGroup()); });
        QObject::connect(project, &QAction::triggered, q,
                         [this] { revealNew(actions->newProject(selectedGroupAndProject().first)); });
        QObject::connect(task, &QAction::triggered, q,
                         [this] { revealNew(actions->newTask(selectedGroupAndProject().second)); });
        button->setMenu(menu);
        return button;
    }

    void revealNew(const QString &id)
    {
        if (!id.isEmpty())
            sidebar->reveal(id);
    }

    void onMoveRequested(SidebarModel::Kind kind, const QString &id, int index)
    {
        switch (kind) {
        case SidebarModel::Kind::Group:
            actions->moveGroup(id, index);
            break;
        case SidebarModel::Kind::Project:
            actions->moveProject(id, index);
            break;
        case SidebarModel::Kind::Task:
            actions->moveTask(id, index);
            break;
        default:
            break;
        }
    }

    static bool isTextInput(const QWidget *widget)
    {
        return qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTextEdit *>(widget)
               || qobject_cast<const QPlainTextEdit *>(widget) || qobject_cast<const QAbstractSpinBox *>(widget);
    }

    static inline const QString focusTabId = QStringLiteral("focus");

    QString workspace;              // the open data folder; empty for the sample data
    QAction *showSidebarAction = nullptr;
    QAction *fullScreenAction = nullptr;
    bool maximizedBeforeFullScreen = false;     // the state full screen returns to
    QTimer *menuPeek = nullptr;                 // runs only in full screen

    // In full screen the menu bar hides, and shows while the mouse is at the
    // top edge of the window or a menu is open, as in web browsers.
    void followFullScreen()
    {
        QMenuBar *bar = q->menuBar();
        if (!q->isFullScreen()) {
            if (menuPeek)
                menuPeek->stop();
            bar->show();
            return;
        }
        if (!menuPeek) {
            menuPeek = new QTimer(q);
            menuPeek->setInterval(100);
            QObject::connect(menuPeek, &QTimer::timeout, q, [this] { peekMenuBar(); });
        }
        bar->hide();
        menuPeek->start();
    }

    void peekMenuBar()
    {
        constexpr int edge = 4;     // pixels at the top that show the bar
        QMenuBar *bar = q->menuBar();
        const QPoint pos = q->mapFromGlobal(QCursor::pos());
        const bool inWindow = q->rect().contains(pos);
        if (!bar->isVisible()) {
            if (inWindow && pos.y() < edge)
                bar->show();
            return;
        }
        const bool menuOpen = bar->activeAction() || QApplication::activePopupWidget();
        if (!menuOpen && !(inWindow && pos.y() < bar->height() + edge))
            bar->hide();
    }

    // A hidden menu bar takes its menus' shortcuts with it, F11 included, so
    // the window holds the actions that have one too.
    void keepMenuShortcuts(const QList<QAction *> &menuActions)
    {
        for (QAction *action : menuActions) {
            if (QMenu *menu = action->menu())
                keepMenuShortcuts(menu->actions());
            else if (!action->shortcut().isEmpty())
                q->addAction(action);
        }
    }

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

    // Opens a workspace from the menus. A folder that exists but is not a
    // workspace can be made one, after asking.
    void openInteractively(const QString &path)
    {
        const QFileInfo info(path);
        const QString folder = info.isFile() ? info.absolutePath() : path;
        if (QDir(folder).exists() && !store::isWorkspace(folder)) {
            const auto answer = QMessageBox::question(
                q, MainWindow::tr("Open Workspace"),
                MainWindow::tr("%1 is not a Mirante workspace: it has no %2. Make it a workspace?")
                    .arg(QDir::toNativeSeparators(folder), store::workspaceFileName()));
            if (answer == QMessageBox::Yes)
                promptCreate(folder);
            return;
        }
        q->openWorkspace(folder);
    }

    // Asks for a name, then makes the folder a workspace and opens it.
    void promptCreate(const QString &folder)
    {
        bool ok = false;
        const QString name = QInputDialog::getText(q, MainWindow::tr("New Workspace"), MainWindow::tr("Name:"),
                                                   QLineEdit::Normal, QDir(folder).dirName(), &ok)
                                 .trimmed();
        if (ok && !name.isEmpty())
            q->createWorkspace(folder, name);
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

    void remember(const QString &folder)
    {
        settings->setLastWorkspace(folder);
        settings->addRecentWorkspace(folder);   // ignores the sample data's empty folder
    }

    // The File, View and Help menus.
    void buildMenus()
    {
        QMenuBar *bar = q->menuBar();

        QMenu *file = bar->addMenu(MainWindow::tr("&File"));
        QAction *newWorkspace = file->addAction(MainWindow::tr("&New Workspace…"));
        newWorkspace->setShortcut(QKeySequence::New);
        QObject::connect(newWorkspace, &QAction::triggered, q, [this] {
            const QString folder = QFileDialog::getExistingDirectory(
                q, MainWindow::tr("New Workspace: choose its folder"), workspace);
            if (folder.isEmpty())
                return;
            if (store::isWorkspace(folder))
                q->openWorkspace(folder);
            else
                promptCreate(folder);
        });
        QAction *open = file->addAction(MainWindow::tr("&Open Workspace…"));
        open->setShortcut(QKeySequence::Open);
        QObject::connect(open, &QAction::triggered, q, [this] {
            const QString path = QFileDialog::getOpenFileName(
                q, MainWindow::tr("Open Workspace"), workspace,
                MainWindow::tr("Mirante workspace (%1)").arg(store::workspaceFileName()));
            if (!path.isEmpty())
                openInteractively(path);
        });
        QMenu *recentMenu = file->addMenu(MainWindow::tr("Open &Recent"));
        QObject::connect(recentMenu, &QMenu::aboutToShow, q, [this, recentMenu] {
            recentMenu->clear();
            const QStringList folders = settings->recentWorkspaces();
            for (const QString &folder : folders) {
                QAction *action = recentMenu->addAction(QDir::toNativeSeparators(folder));
                QObject::connect(action, &QAction::triggered, q, [this, folder] { openInteractively(folder); });
            }
            if (folders.isEmpty())
                recentMenu->addAction(MainWindow::tr("No recent workspaces"))->setEnabled(false);
            recentMenu->addSeparator();
            QAction *clear = recentMenu->addAction(MainWindow::tr("Clear Recent"));
            clear->setEnabled(!folders.isEmpty());
            QObject::connect(clear, &QAction::triggered, q, [this] { settings->clearRecentWorkspaces(); });
        });
        QAction *rename = file->addAction(MainWindow::tr("Re&name Workspace…"));
        QObject::connect(rename, &QAction::triggered, q, [this] { promptRename(); });
        // Opens the folder in Explorer, Finder or the desktop's file manager.
        QAction *showFolder = file->addAction(MainWindow::tr("Show Workspace &Folder"));
        QObject::connect(showFolder, &QAction::triggered, q,
                         [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(workspace)); });
        QObject::connect(file, &QMenu::aboutToShow, q, [this, rename, showFolder] {
            rename->setEnabled(!workspace.isEmpty());
            showFolder->setEnabled(!workspace.isEmpty());
        });
        QAction *sample = file->addAction(MainWindow::tr("Open &Sample Data"));
        QObject::connect(sample, &QAction::triggered, q, &MainWindow::openSampleData);
        QAction *reload = file->addAction(MainWindow::tr("Re&load Workspace"));
        reload->setShortcut(QKeySequence::Refresh);
        QObject::connect(reload, &QAction::triggered, q, &MainWindow::reloadWorkspace);
        file->addSeparator();
        QAction *newGroup = file->addAction(MainWindow::tr("New &Group…"));
        QObject::connect(newGroup, &QAction::triggered, q, [this] { revealNew(actions->newGroup()); });
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
        // The platform's full screen key, as in web browsers: F11 on Windows
        // and Linux, Ctrl+Cmd+F on macOS.
        fullScreenAction = view->addAction(MainWindow::tr("&Full Screen"));
        fullScreenAction->setCheckable(true);
        fullScreenAction->setShortcut(QKeySequence::FullScreen);
        QObject::connect(fullScreenAction, &QAction::triggered, q, [this](bool on) { q->setFullScreen(on); });
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
        // Debug builds, and release builds started with --log, keep the log
        // messages for a window. See main.cpp.
        if (LogBuffer::installedBuffer()) {
            help->addSeparator();
            QAction *log = help->addAction(MainWindow::tr("Show &Log"));
            log->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
            QObject::connect(log, &QAction::triggered, q, [this] { showLog(); });
        }
    }

    // Opens the log window, or brings it to the front. It has no parent, so it
    // can sit beside the main window.
    void showLog()
    {
        LogBuffer *buffer = LogBuffer::installedBuffer();
        if (!buffer)
            return;
        if (!logWindow) {
            logWindow = new LogWindow(*buffer);
            logWindow->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(q, &QObject::destroyed, logWindow.data(), &QObject::deleteLater);
        }
        logWindow->show();
        logWindow->raise();
        logWindow->activateWindow();
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
        warningBar->setFrameShape(QFrame::NoFrame);
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
        layout->addWidget(new ConflictBar(*store, *model, content));
        layout->addLayout(tabRow);
        layout->addWidget(stack, 1);
        return content;
    }
};

MainWindow::MainWindow(Model &model, const ItemTypeRegistry &types, Store &store, DeviceSettings &settings,
                       ProfileManager *profiles, ViewPool *pool, QWidget *parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->model = &model;
    d->types = &types;
    d->store = &store;
    d->settings = &settings;
    d->profiles = profiles;
    d->pool = pool;

    d->editor = new LayoutEditor(model, types, this);
    d->locks = new LockState(this);
    d->actions = new EntityActions(model, store, types, settings, this);
    // A new task dashboard opens in the preview tab, ready to fill.
    connect(d->actions, &EntityActions::dashboardCreated, this,
            [this](const QString &taskId, const QString &dashboardId) { openTaskPreview(taskId, dashboardId); });
    d->imagePaste = std::make_unique<ImagePaste>(model, store, types, *d->locks);
    d->tracker = new SingleViewTracker(this);
    d->services.model = &model;
    d->services.types = &types;
    d->services.editor = d->editor;
    d->services.locks = d->locks;
    d->services.tracker = d->tracker;
    d->services.hover = new CellHoverTracker(this);
    d->services.openTaskAlone = [this](const QString &taskId, const QString &dashboardId) {
        openTaskPreview(taskId, dashboardId);
    };
    // Item cells show their views only with profiles. Without them, as in
    // tests, they keep their placeholders.
    if (profiles) {
        d->placements.model = &model;
        d->placements.types = &types;
        d->placements.profiles = profiles;
        d->placements.pool = pool;
        d->placements.settings = &settings;
        d->services.placements = &d->placements;
    }
    // A bigger view of an item, from its cell's button and menu. A cell in a
    // detached window is maximised within that window; see
    // docs/ui.md#detached-windows. Shortcuts is made last, after the first
    // pages, so it may not exist yet.
    d->services.toggleMaximised = [this](QWidget *cell) {
        if (d->shortcuts)
            d->shortcuts->toggleMaximised(cell);
    };
    d->services.isMaximised = [this](const QWidget *cell) { return d->shortcuts && d->shortcuts->isMaximised(cell); };
    // From a detached window, the main window comes to the front to show it.
    d->services.openItemAlone = [this](const QString &taskId, const QString &itemId) {
        openItem(taskId, itemId);
        if (!isActiveWindow()) {
            raise();
            activateWindow();
        }
    };
    d->services.editItem = [this](const QString &taskId, const QString &itemId) {
        d->actions->editItem(taskId, itemId);
    };
    d->editor->setServices(&d->services);
    d->actions->setProfiles(profiles);

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
    const QSize size = settings.windowSize();
    if (size.isValid())
        resize(size);
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        QRect frame(QPoint(0, 0), this->size());
        frame.moveCenter(screen->availableGeometry().center());
        move(frame.topLeft());
    }
    if (settings.windowMaximized())
        setWindowState(windowState() | Qt::WindowMaximized);
    d->splitter->restoreState(settings.sidebarSplitterState());
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
    connect(d->sidebar, &Sidebar::focusRequested, this, &MainWindow::openFocus);
    // The sidebar tints the row of what the hovered dashboard cell shows.
    connect(d->services.hover, &CellHoverTracker::hoveredChanged, d->sidebar, &Sidebar::setHoverHighlight);
    d->sidebar->setMenuFiller([this](QMenu *menu, SidebarModel::Kind kind, const QString &id, const QString &taskId) {
        d->actions->fillMenu(menu, kind, id, taskId);
    });
    d->sidebar->setTrashMenuFiller(
        [this](QMenu *menu, SidebarModel::Kind kind, const QString &id, const QString &taskId) {
            d->actions->fillTrashMenu(menu, kind, id, taskId);
        });
    connect(d->sidebar, &Sidebar::emptyTrashRequested, d->actions, &EntityActions::emptyTrash);
    connect(d->sidebar->model(), &SidebarModel::moveRequested, this,
            [this](SidebarModel::Kind kind, const QString &id, int index) { d->onMoveRequested(kind, id, index); });
    d->sidebar->addHeaderWidget(d->buildNewButton());
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
    connect(d->tabBar, &RootTabBar::detachRequested, this, [this](int index) { d->detach(index); });
    connect(d->tabBar, &RootTabBar::renameRequested, this, [this](int index) { d->renameRoot(index); });
    connect(d->listButton, &QToolButton::clicked, this, &MainWindow::showDashboardList);

    // Locks.
    connect(d->lockButton, &QToolButton::clicked, this, [this] {
        const DashboardPage *page = d->currentPage();
        if (page && page->isRoot())
            d->locks->setUnlocked(page->ref.dashboardId, !d->locks->isUnlocked(page->ref.dashboardId));
    });
    connect(d->locks, &LockState::changed, this,
            [this](const QString &id, bool unlocked) { d->applyLock(id, unlocked); });

    // The model.
    connect(&model, &Model::rootDashboardChanged, this, [this](const QString &id) { d->onRootDashboardChanged(id); });
    // A deleted root dashboard loses its tab, or its detached window. Closing
    // a tab does not delete it.
    connect(&model, &Model::rootDashboardRemoved, this, [this](const QString &id) {
        if (DashboardPage *page = d->rootPage(id))
            d->removeTab(d->indexOf(page));
        if (DetachedWindow *window = d->detachedWindow(id))
            d->deleteWindow(window);
    });
    connect(&model, &Model::taskChanged, this, [this](const QString &taskId) { d->onTaskChanged(taskId); });
    connect(&model, &Model::taskRemoved, this, [this](const QString &taskId) { d->onTaskRemoved(taskId); });
    // Task pages name their project.
    connect(&model, &Model::projectChanged, this, [this] {
        d->closeTrashedPages();
        for (std::size_t i = 0; i < d->pages.size(); ++i) {
            d->tabBar->setTabToolTip(int(i), d->pageToolTip(d->pages[i]));
            d->updatePageHeader(d->pages[i]);
        }
    });
    // The pages are made again on a reset. Detached root dashboards that still
    // exist are detached again.
    connect(&model, &Model::modelReset, this, [this] {
        const QStringList detached = d->detachedIds();
        d->closeDetachedWindows();
        d->closeAllTabs();
        d->openInitialTabs();
        for (const QString &id : detached) {
            if (DashboardPage *page = d->rootPage(id))
                d->openInWindow(d->takeTab(page));
        }
    });

    // The Store: its problems and the read-only note go to the warning bar.
    connect(&store, &Store::problemsChanged, this, [this] { d->updateWarnings(); });

    // Ctrl+V with an image on the clipboard pastes it. A text input keeps
    // its own paste: it takes the key before window shortcuts.
    auto *paste = new QShortcut(QKeySequence::Paste, this);
    paste->setContext(Qt::WindowShortcut);
    connect(paste, &QShortcut::activated, this, [this] {
        if (Private::isTextInput(QApplication::focusWidget()))
            return;
        const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
        if (!mime || !mime->hasImage())
            return;
        const QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull())
            pasteImage(image);
    });

    // The switcher and the shortcuts.
    d->switcher = new QuickSwitcher(model, this);
    connect(d->switcher, &QuickSwitcher::activated, this,
            [this](const SwitcherEntry &entry) { d->onSwitcherActivated(entry); });
    d->buildMenus();
    d->keepMenuShortcuts(menuBar()->actions());

    d->openInitialTabs();
    d->shortcuts = new Shortcuts(this);
}

MainWindow::~MainWindow()
{
    // Lambdas connected here read d, which goes before QWidget's destructor
    // deletes the children. Disconnect them all first.
    disconnect(d->model, nullptr, this, nullptr);
    disconnect(d->store, nullptr, this, nullptr);
    const auto children = findChildren<QObject *>();
    for (QObject *child : children)
        child->disconnect(this);

    // The detached windows are not children. They go first, with their pages,
    // while the services their pages use still exist.
    for (DetachedWindow *window : d->detached) {
        window->disconnect(this);
        delete window;
    }
    d->detached.clear();

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
    // The size of the normal, not maximized, window is kept. A full screen
    // window is saved as the window it came from.
    d->settings->setWindowSize(normalGeometry().size());
    d->settings->setWindowMaximized(isFullScreen() ? d->maximizedBeforeFullScreen : isMaximized());
    d->settings->setSidebarSplitterState(d->splitter->saveState());
    d->saveOpenTabs();
    // After the open tabs are saved, so the windows open again next time.
    d->putBackAll();
    QMainWindow::closeEvent(event);
}

void MainWindow::setFullScreen(bool on)
{
    if (on == isFullScreen())
        return;
    if (on) {
        d->maximizedBeforeFullScreen = isMaximized();
        showFullScreen();
    } else if (d->maximizedBeforeFullScreen) {
        showMaximized();
    } else {
        showNormal();
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    // The window can also leave full screen without the action, for example
    // through the macOS title bar.
    if (event->type() == QEvent::WindowStateChange && d->fullScreenAction) {
        d->fullScreenAction->setChecked(isFullScreen());
        d->followFullScreen();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setLoadProblems(const std::vector<LoadProblem> &problems)
{
    d->ownProblems = problems;
    d->updateWarnings();
}

void MainWindow::openWorkspace(const QString &path)
{
    // The path of workspace.mirante stands for its folder.
    const QFileInfo info(path);
    const QString folder = info.isFile() ? info.absolutePath() : path;
    if (folder.isEmpty() || !QDir(folder).exists()) {
        d->settings->removeRecentWorkspace(folder);
        setLoadProblems({LoadProblem{QDir::toNativeSeparators(folder), tr("The workspace folder does not exist.")}});
        return;
    }
    if (!store::isWorkspace(folder)) {
        setLoadProblems({LoadProblem{QDir::toNativeSeparators(folder),
                                     tr("not a Mirante workspace: it has no %1").arg(store::workspaceFileName())}});
        return;
    }
    d->workspace = QDir(folder).absolutePath();
    d->openThroughStore(d->workspace);
    d->remember(d->workspace);
    d->updateTitle();
}

bool MainWindow::createWorkspace(const QString &folder, const QString &name)
{
    QString error;
    if (name.trimmed().isEmpty() || !store::createWorkspace(folder, name.trimmed(), &error)) {
        setLoadProblems({LoadProblem{QDir::toNativeSeparators(folder),
                                     tr("cannot be made a workspace: %1").arg(error.isEmpty() ? tr("no name") : error)}});
        return false;
    }
    openWorkspace(folder);
    return true;
}

void MainWindow::openSampleData()
{
    d->workspace.clear();
    d->openThroughStore(QStringLiteral(":/fixtures"));
    d->remember(QString());
    d->updateTitle();
}

void MainWindow::reloadWorkspace()
{
    if (d->workspace.isEmpty())
        openSampleData();
    else
        openWorkspace(d->workspace);
}

bool MainWindow::renameWorkspace(const QString &name)
{
    // The Store writes workspace.mirante when the model's name changes, and
    // reports a failed write in the warning bar.
    if (d->workspace.isEmpty() || d->store->readOnly() || name.trimmed().isEmpty())
        return false;
    d->model->setWorkspaceName(name.trimmed());
    return true;
}

QString MainWindow::workspace() const { return d->workspace; }

QStringList MainWindow::recentWorkspaces() const { return d->settings->recentWorkspaces(); }

void MainWindow::showAbout()
{
    // The versions of Qt and of the browser engine help when a login fails.
    QMessageBox::about(
        this, tr("About %1").arg(QStringLiteral(MIRANTE_APP_NAME)),
        tr("<h3>%1 %2</h3><p>%3</p><p>%4 &lt;<a href=\"mailto:%5\">%5</a>&gt;</p>"
           "<p><a href=\"%6\">%6</a></p><p>Released under the MIT License.</p>")
                .arg(QStringLiteral(MIRANTE_APP_NAME), QStringLiteral(MIRANTE_VERSION),
                     QStringLiteral(MIRANTE_DESCRIPTION), QStringLiteral(MIRANTE_AUTHOR),
                     QStringLiteral(MIRANTE_AUTHOR_EMAIL), QStringLiteral(MIRANTE_HOMEPAGE))
            + tr("<p>Qt %1, Qt WebEngine %2, Chromium %3</p>")
                  .arg(QString::fromLatin1(qVersion()), QString::fromLatin1(qWebEngineVersion()),
                       QString::fromLatin1(qWebEngineChromiumVersion())));
}

void MainWindow::openTaskPreview(const QString &taskId, const QString &chosenDashboardId)
{
    const Task *task = d->model->task(taskId);
    if (!task || d->model->taskInTrash(*task))
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
    if (!task || d->model->taskInTrash(*task))
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
    // A detached root dashboard is shown in its window.
    if (DetachedWindow *window = d->detachedWindow(id)) {
        if (window->isMinimized())
            window->showNormal();
        window->show();
        window->raise();
        window->activateWindow();
        return;
    }
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
            *d->model, [this](const QString &id) { return d->rootPage(id) || d->detachedWindow(id); }, this);
        d->dashboardList->setAttribute(Qt::WA_DeleteOnClose);
        connect(d->dashboardList, &RootDashboardsDialog::openRequested, this, &MainWindow::openRootDashboard);
    }
    d->dashboardList->show();
    d->dashboardList->raise();
    d->dashboardList->activateWindow();
}

void MainWindow::openItem(const QString &taskId, const QString &itemId)
{
    if (!d->model->item(taskId, itemId) || d->model->taskInTrash(taskId))
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

void MainWindow::openFocus()
{
    DashboardPage *page = d->focusPage();
    if (!page) {
        page = d->makeFocusPage();
        d->insertTab(0, page);
    }
    d->setCurrent(page);
}

void MainWindow::pasteImage(const QImage &image)
{
    const DashboardPage *page = d->currentPage();
    if (!page || page->isFocus())
        return;
    ImagePaste::Result result;
    if (!page->itemId.isEmpty()) {
        result = d->imagePaste->pasteIntoItem(image, page->ref.taskId, page->itemId);
    } else {
        std::optional<NodePath> path;
        if (const QWidget *cell = d->shortcuts->focusedCell())
            path = page->view->pathOf(cell);
        result = d->imagePaste->paste(image, page->ref, path);
        // A root dashboard has no task of its own, so the image needs one.
        if (result == ImagePaste::Result::RootDashboard) {
            const std::optional<QString> taskId = d->askTaskForImage();
            if (!taskId)
                return;
            if (!taskId->isEmpty())
                result = d->imagePaste->paste(image, page->ref, path, *taskId);
        }
    }
    const QString message = ImagePaste::message(result);
    if (!message.isEmpty())
        statusBar()->showMessage(message, 5000);
}

void MainWindow::saveOpenTabs() { d->saveOpenTabs(); }

EntityActions *MainWindow::entityActions() const { return d->actions; }

RootTabBar *MainWindow::rootTabBar() const { return d->tabBar; }

LayoutView *MainWindow::currentLayoutView() const
{
    const DashboardPage *page = d->currentPage();
    return page ? page->view : nullptr;
}

QWidget *MainWindow::contentArea() const { return d->stack; }

Shortcuts *MainWindow::shortcuts() const { return d->shortcuts; }

} // namespace mirante
