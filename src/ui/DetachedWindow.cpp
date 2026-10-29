#include "ui/DetachedWindow.h"

#include "core/DeviceSettings.h"
#include "layout/LockState.h"
#include "ui/DashboardPage.h"

#include <QCloseEvent>
#include <QVBoxLayout>

namespace mirante {

// Stub from M4 T0. T8 implements it.
struct DetachedWindow::Private
{
    DashboardPage *page = nullptr;
    LockState *locks = nullptr;
    DeviceSettings *settings = nullptr;
    QString dashboardId;
};

DetachedWindow::DetachedWindow(DashboardPage *page, LockState *locks, DeviceSettings &settings, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , d(std::make_unique<Private>(Private{page, locks, &settings, page ? page->ref.dashboardId : QString()}))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    if (page)
        layout->addWidget(page);
}

DetachedWindow::~DetachedWindow() = default;

QString DetachedWindow::dashboardId() const { return d->dashboardId; }

DashboardPage *DetachedWindow::page() const { return d->page; }

DashboardPage *DetachedWindow::takePage()
{
    DashboardPage *page = d->page;
    d->page = nullptr;
    if (page)
        page->setParent(nullptr);
    return page;
}

void DetachedWindow::setTitle(const QString &name) { setWindowTitle(name); }

void DetachedWindow::closeEvent(QCloseEvent *event)
{
    emit putBackRequested(d->dashboardId);
    event->accept();
}

} // namespace mirante
