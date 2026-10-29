#pragma once

#include <QString>
#include <QWidget>

#include <memory>

namespace mirante {

class DashboardPage;
class DeviceSettings;
class LockState;

// A top-level window holding one root dashboard, with a toolbar for its lock
// and "Put Back". Closing the window also puts the dashboard back. Its
// geometry is stored on the device. See
// docs/decisions/0024-detach-root-dashboard-tabs.md and the M4 plan, part 8.
// Implemented in DetachedWindow.cpp.
class DetachedWindow : public QWidget
{
    Q_OBJECT

public:
    // Takes the page of a root dashboard. The page is moved, not made again,
    // so its web pages keep their state. locks and settings must outlive the
    // window.
    DetachedWindow(DashboardPage *page, LockState *locks, DeviceSettings &settings, QWidget *parent = nullptr);
    ~DetachedWindow() override;

    QString dashboardId() const;
    DashboardPage *page() const;

    // Gives the page back, for the main window to put in its tab bar. The
    // window is empty afterwards and should be deleted.
    DashboardPage *takePage();

    // Follows a rename of the root dashboard.
    void setTitle(const QString &name);

signals:
    // The window was closed or "Put Back" was clicked.
    void putBackRequested(const QString &dashboardId);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
