#include "ui/DetachedWindow.h"

#include "core/DeviceSettings.h"
#include "layout/LayoutView.h"
#include "layout/LockState.h"
#include "ui/DashboardPage.h"
#include "ui/Shortcuts.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QPointer>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

namespace mirante {

namespace {

// The size of a window with no stored geometry.
constexpr int defaultWidth = 1000;
constexpr int defaultHeight = 700;

} // namespace

struct DetachedWindow::Private
{
    DashboardPage *page = nullptr;
    // A window that was put back is deleted later, maybe after the locks.
    QPointer<LockState> locks;
    DeviceSettings *settings = nullptr;
    QString dashboardId;
    QToolButton *lock = nullptr;

    // Shows the lock's state as the main window's lock does: the icon shows
    // the state, the text is the button's accessible name, and the tooltip
    // names the lock key.
    void showLockState(bool unlocked)
    {
        const QString key = lockShortcut().toString(QKeySequence::NativeText);
        lock->setIcon(QIcon(unlocked ? QStringLiteral(":/icons/lock-open.svg") : QStringLiteral(":/icons/lock-closed.svg")));
        lock->setText(unlocked ? DetachedWindow::tr("Editing") : DetachedWindow::tr("Locked"));
        lock->setToolTip(unlocked ? DetachedWindow::tr("Lock this dashboard (%1)").arg(key)
                                  : DetachedWindow::tr("Unlock this dashboard for editing (%1)").arg(key));
    }

    // The page is in edit mode while its dashboard is unlocked.
    void applyLock(bool unlocked)
    {
        showLockState(unlocked);
        if (page && page->view)
            page->view->setEditMode(unlocked);
    }
};

DetachedWindow::DetachedWindow(DashboardPage *page, LockState *locks, DeviceSettings &settings, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , d(std::make_unique<Private>(Private{page, locks, &settings, page ? page->ref.dashboardId : QString()}))
{
    // The toolbar: "Put Back", then the dashboard's lock at the far right, as
    // in the main window's tab row.
    auto *putBack = new QToolButton(this);
    putBack->setObjectName(QStringLiteral("putBackButton"));
    putBack->setText(tr("Put Back"));
    putBack->setToolTip(tr("Put this dashboard back in the main window"));
    putBack->setAutoRaise(true);
    connect(putBack, &QToolButton::clicked, this, &QWidget::close);

    d->lock = new QToolButton(this);
    d->lock->setObjectName(QStringLiteral("lockButton"));
    d->lock->setAutoRaise(true);
    connect(d->lock, &QToolButton::clicked, this, [this] {
        if (d->locks)
            d->locks->setUnlocked(d->dashboardId, !d->locks->isUnlocked(d->dashboardId));
    });

    // The lock key works as the button does. The main window's shortcuts
    // act in the main window only.
    auto *lockKey = new QShortcut(lockShortcut(), this);
    lockKey->setContext(Qt::WindowShortcut);
    connect(lockKey, &QShortcut::activated, d->lock, &QToolButton::click);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 4, 4, 0);
    toolbar->setSpacing(2);
    toolbar->addWidget(putBack);
    toolbar->addStretch(1);
    toolbar->addWidget(d->lock);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(toolbar);
    if (page) {
        layout->addWidget(page, 1);
        page->show();
    }

    if (d->locks) {
        connect(d->locks, &LockState::changed, this, [this](const QString &id, bool unlocked) {
            if (id == d->dashboardId)
                d->applyLock(unlocked);
        });
    }
    d->applyLock(d->locks && d->locks->isUnlocked(d->dashboardId));

    // The stored geometry, or a default size.
    const QByteArray geometry = settings.detachedGeometry(d->dashboardId);
    if (geometry.isEmpty() || !restoreGeometry(geometry))
        resize(defaultWidth, defaultHeight);
}

DetachedWindow::~DetachedWindow()
{
    // The lambdas read d, which goes before QWidget's destructor.
    if (d->locks)
        disconnect(d->locks, nullptr, this, nullptr);
}

QString DetachedWindow::dashboardId() const { return d->dashboardId; }

DashboardPage *DetachedWindow::page() const { return d->page; }

DashboardPage *DetachedWindow::takePage()
{
    DashboardPage *page = d->page;
    d->page = nullptr;
    if (page) {
        layout()->removeWidget(page);
        page->setParent(nullptr);
    }
    return page;
}

void DetachedWindow::setTitle(const QString &name) { setWindowTitle(name); }

void DetachedWindow::closeEvent(QCloseEvent *event)
{
    d->settings->setDetachedGeometry(d->dashboardId, saveGeometry());
    event->accept();
    emit putBackRequested(d->dashboardId);
}

} // namespace mirante
