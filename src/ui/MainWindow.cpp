// Stub from task T0. Task T15 implements this file.
#include "ui/MainWindow.h"

#include "Version.h"

namespace mirante {

struct MainWindow::Private
{
};

MainWindow::MainWindow(Model & /*model*/, const ItemTypeRegistry & /*types*/, QWidget *parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    setWindowTitle(QStringLiteral(MIRANTE_APP_NAME));
    resize(1280, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::setLoadProblems(const std::vector<LoadProblem> & /*problems*/) {}

void MainWindow::openTaskPreview(const QString & /*taskId*/) {}
void MainWindow::pinTask(const QString & /*taskId*/) {}
void MainWindow::openRootDashboard(const QString & /*id*/) {}
void MainWindow::newRootDashboard() {}
void MainWindow::focusItem(const QString & /*taskId*/, const QString & /*itemId*/) {}
void MainWindow::revealInSidebar(const QString & /*id*/) {}
void MainWindow::showQuickSwitcher() {}
void MainWindow::closeCurrentTab() {}

RootTabBar *MainWindow::rootTabBar() const { return nullptr; }
LayoutView *MainWindow::currentLayoutView() const { return nullptr; }
QWidget *MainWindow::contentArea() const { return nullptr; }
Shortcuts *MainWindow::shortcuts() const { return nullptr; }

} // namespace mirante
